void getColumns(PetscReal **globalIndices, PetscInt i, PetscInt j, PetscInt *cols)
{
	cols[0] = globalIndices[j][i];
	cols[1] = globalIndices[j][i-1];
	cols[2] = globalIndices[j][i+1];
	cols[3] = globalIndices[j-1][i];	
	cols[4] = globalIndices[j+1][i];
}

void getColumns(PetscReal ***globalIndices, PetscInt i, PetscInt j, PetscInt k, PetscInt *cols)
{
	cols[0] = globalIndices[k][j][i];
	cols[1] = globalIndices[k][j][i-1];
	cols[2] = globalIndices[k][j][i+1];
	cols[3] = globalIndices[k][j-1][i];	
	cols[4] = globalIndices[k][j+1][i];
	cols[5] = globalIndices[k-1][j][i];
	cols[6] = globalIndices[k+1][j][i];
}

void getCoefficients(PetscReal dxMinus, PetscReal dxPlus, PetscReal dyMinus, PetscReal dyPlus, PetscReal *values)
{
	values[0] = -(2.0/dxMinus/dxPlus + 2.0/dyMinus/dyPlus);
	values[1] = 2.0/dxMinus/(dxMinus + dxPlus);
	values[2] = 2.0/ dxPlus/(dxMinus + dxPlus);
	values[3] = 2.0/dyMinus/(dyMinus + dyPlus);
	values[4] = 2.0/ dyPlus/(dyMinus + dyPlus);
}

void getCoefficients(PetscReal dxMinus, PetscReal dxPlus, PetscReal dyMinus, PetscReal dyPlus, PetscReal dzMinus, PetscReal dzPlus, PetscReal *values)
{
	getCoefficients(dxMinus, dxPlus, dyMinus, dyPlus, values);	
	values[0] += (-2.0/dzMinus/dzPlus);
	values[5] = 2.0/dzMinus/(dzMinus + dzPlus);
	values[6] = 2.0/ dzPlus/(dzMinus + dzPlus);
}

template <>
PetscErrorCode NavierStokesSolver<2>::generateA()
{
	PetscErrorCode ierr;
	PetscInt       mstart, nstart, m, n, i, j;
	PetscInt       cols[5];
	PetscReal      values[5];
	PetscReal      **uGlobalIdx, **vGlobalIdx;
	PetscInt       qStart, qEnd, qLocalSize;
	PetscInt       *d_nnz, *o_nnz;
	PetscInt       localIdx;
	PetscReal      dt = simParams->dt,
	               nu = flowDesc->nu,
	               alphaImplicit = simParams->alphaImplicit;

	// ownership range of q
	ierr = VecGetOwnershipRange(q, &qStart, &qEnd); CHKERRQ(ierr);
	qLocalSize = qEnd-qStart;

	// create arrays to store nnz values
	ierr = PetscMalloc(qLocalSize*sizeof(PetscInt), &d_nnz); CHKERRQ(ierr);
	ierr = PetscMalloc(qLocalSize*sizeof(PetscInt), &o_nnz); CHKERRQ(ierr);

	// determine the number of non-zeros in each row
	// in the diagonal and off-diagonal portions of the matrix
	localIdx = 0;
	// U
	ierr = DMDAVecGetArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(uda, &mstart, &nstart, NULL, &m, &n, NULL); CHKERRQ(ierr);
	for(j=nstart; j<nstart+n; j++)
	{
		for(i=mstart; i<mstart+m; i++)
		{
			getColumns(uGlobalIdx, i, j, cols);
			countNumNonZeros(cols, 5, qStart, qEnd, d_nnz[localIdx], o_nnz[localIdx]);
			localIdx++;
		}
	}
	ierr = DMDAVecRestoreArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	// V
	ierr = DMDAVecGetArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(vda, &mstart, &nstart, NULL, &m, &n, NULL); CHKERRQ(ierr);
	for(j=nstart; j<nstart+n; j++)
	{
		for(i=mstart; i<mstart+m; i++)
		{
			getColumns(vGlobalIdx, i, j, cols);
			countNumNonZeros(cols, 5, qStart, qEnd, d_nnz[localIdx], o_nnz[localIdx]);
			localIdx++;
		}
	}
	ierr = DMDAVecRestoreArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);

	// create and allocate memory for matrix A
	ierr = MatCreate(PETSC_COMM_WORLD, &A); CHKERRQ(ierr);
	ierr = MatSetSizes(A, qLocalSize, qLocalSize, PETSC_DETERMINE, PETSC_DETERMINE); CHKERRQ(ierr);
	ierr = MatSetFromOptions(A); CHKERRQ(ierr);
	ierr = MatSeqAIJSetPreallocation(A, 0, d_nnz); CHKERRQ(ierr);
	ierr = MatMPIAIJSetPreallocation(A, 0, d_nnz, 0, o_nnz); CHKERRQ(ierr);

	// deallocate d_nnz and o_nnz
	ierr = PetscFree(d_nnz); CHKERRQ(ierr);
	ierr = PetscFree(o_nnz); CHKERRQ(ierr);

	// assemble matrix A
	// U
	ierr = DMDAVecGetArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(uda, &mstart, &nstart, NULL, &m, &n, NULL); CHKERRQ(ierr);
	for(j=nstart; j<nstart+n; j++)
	{
		for(i=mstart; i<mstart+m; i++)
		{
			getColumns(uGlobalIdx, i, j, cols);
			getCoefficients(dxU[i], dxU[i+1], dyU[j], dyU[j+1], values);
			ierr = MatSetValues(A, 1, &cols[0], 5, cols, values, INSERT_VALUES); CHKERRQ(ierr);
		}
	}
	ierr = DMDAVecRestoreArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	// V
	ierr = DMDAVecGetArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(vda, &mstart, &nstart, NULL, &m, &n, NULL); CHKERRQ(ierr);
	for(j=nstart; j<nstart+n; j++)
	{
		for(i=mstart; i<mstart+m; i++)
		{
			getColumns(vGlobalIdx, i, j, cols);
			getCoefficients(dxV[i], dxV[i+1], dyV[j], dyV[j+1], values);
			ierr = MatSetValues(A, 1, &cols[0], 5, cols, values, INSERT_VALUES); CHKERRQ(ierr);
		}
	}
	ierr = DMDAVecRestoreArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);

	ierr = MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
	ierr = MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

	ierr = MatScale(A, nu*alphaImplicit); CHKERRQ(ierr);
	ierr = MatShift(A, -1.0/dt); CHKERRQ(ierr);
	ierr = MatScale(A, -1.0); CHKERRQ(ierr);
	ierr = MatDiagonalScale(A, MHat, RInv);

	return 0;
}

template <>
PetscErrorCode NavierStokesSolver<3>::generateA()
{
	PetscErrorCode ierr;
	PetscInt       mstart, nstart, pstart, m, n, p, i, j, k;
	PetscInt       cols[7];
	PetscReal      values[7];
	PetscReal      ***uGlobalIdx, ***vGlobalIdx, ***wGlobalIdx;
	PetscInt       qStart, qEnd, qLocalSize;
	PetscInt       *d_nnz, *o_nnz;
	PetscInt       localIdx;
	PetscReal      dt = simParams->dt,
	               nu = flowDesc->nu,
	               alphaImplicit = simParams->alphaImplicit;

	// ownership range of q
	ierr = VecGetOwnershipRange(q, &qStart, &qEnd); CHKERRQ(ierr);
	qLocalSize = qEnd-qStart;

	// create arrays to store nnz values
	ierr = PetscMalloc(qLocalSize*sizeof(PetscInt), &d_nnz); CHKERRQ(ierr);
	ierr = PetscMalloc(qLocalSize*sizeof(PetscInt), &o_nnz); CHKERRQ(ierr);

	// determine the number of non-zeros in each row
	// in the diagonal and off-diagonal portions of the matrix
	localIdx = 0;
	// U
	ierr = DMDAVecGetArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(uda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(uGlobalIdx, i, j, k, cols);
				countNumNonZeros(cols, 7, qStart, qEnd, d_nnz[localIdx], o_nnz[localIdx]);
				localIdx++;
			}
		}
	}
	ierr = DMDAVecRestoreArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	// V
	ierr = DMDAVecGetArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(vda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(vGlobalIdx, i, j, k, cols);
				countNumNonZeros(cols, 7, qStart, qEnd, d_nnz[localIdx], o_nnz[localIdx]);
				localIdx++;
			}
		}
	}
	ierr = DMDAVecRestoreArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	// W
	ierr = DMDAVecGetArray(wda, wMapping, &wGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(wda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(wGlobalIdx, i, j, k, cols);
				countNumNonZeros(cols, 7, qStart, qEnd, d_nnz[localIdx], o_nnz[localIdx]);
				localIdx++;
			}
		}
	}
	ierr = DMDAVecRestoreArray(wda, wMapping, &wGlobalIdx); CHKERRQ(ierr);

	// create and allocate memory for matrix A
	ierr = MatCreate(PETSC_COMM_WORLD, &A); CHKERRQ(ierr);
	ierr = MatSetSizes(A, qLocalSize, qLocalSize, PETSC_DETERMINE, PETSC_DETERMINE); CHKERRQ(ierr);
	ierr = MatSetFromOptions(A); CHKERRQ(ierr);
	ierr = MatSeqAIJSetPreallocation(A, 0, d_nnz); CHKERRQ(ierr);
	ierr = MatMPIAIJSetPreallocation(A, 0, d_nnz, 0, o_nnz); CHKERRQ(ierr);

	// deallocate d_nnz and o_nnz
	ierr = PetscFree(d_nnz); CHKERRQ(ierr);
	ierr = PetscFree(o_nnz); CHKERRQ(ierr);

	// assemble matrix A
	// U
	ierr = DMDAVecGetArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(uda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(uGlobalIdx, i, j, k, cols);
				getCoefficients(dxU[i], dxU[i+1], dyU[j], dyU[j+1], dzU[k], dzU[k+1], values);
				ierr = MatSetValues(A, 1, &cols[0], 7, cols, values, INSERT_VALUES); CHKERRQ(ierr);
			}
		}
	}
	ierr = DMDAVecRestoreArray(uda, uMapping, &uGlobalIdx); CHKERRQ(ierr);
	// V
	ierr = DMDAVecGetArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(vda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(vGlobalIdx, i, j, k, cols);
				getCoefficients(dxV[i], dxV[i+1], dyV[j], dyV[j+1], dzV[k], dzV[k+1], values);
				ierr = MatSetValues(A, 1, &cols[0], 7, cols, values, INSERT_VALUES); CHKERRQ(ierr);
			}
		}
	}
	ierr = DMDAVecRestoreArray(vda, vMapping, &vGlobalIdx); CHKERRQ(ierr);
	// W
	ierr = DMDAVecGetArray(wda, wMapping, &wGlobalIdx); CHKERRQ(ierr);
	ierr = DMDAGetCorners(wda, &mstart, &nstart, &pstart, &m, &n, &p); CHKERRQ(ierr);
	for(k=pstart; k<pstart+p; k++)
	{
		for(j=nstart; j<nstart+n; j++)
		{
			for(i=mstart; i<mstart+m; i++)
			{
				getColumns(wGlobalIdx, i, j, k, cols);
				getCoefficients(dxW[i], dxW[i+1], dyW[j], dyW[j+1], dzW[k], dzW[k+1], values);
				ierr = MatSetValues(A, 1, &cols[0], 7, cols, values, INSERT_VALUES); CHKERRQ(ierr);
			}
		}
	}
	ierr = DMDAVecRestoreArray(wda, wMapping, &wGlobalIdx); CHKERRQ(ierr);

	ierr = MatAssemblyBegin(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);
	ierr = MatAssemblyEnd(A, MAT_FINAL_ASSEMBLY); CHKERRQ(ierr);

	ierr = MatScale(A, nu*alphaImplicit); CHKERRQ(ierr);
	ierr = MatShift(A, -1.0/dt); CHKERRQ(ierr);
	ierr = MatScale(A, -1.0); CHKERRQ(ierr);
	ierr = MatDiagonalScale(A, MHat, RInv); CHKERRQ(ierr);

	return 0;
}
