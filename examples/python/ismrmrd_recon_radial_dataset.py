#!/usr/bin/python
# -*- coding: utf-8 -*-

# ismrmrd_recon_radial_dataset.py
#
# Copyright 2014 Ghislain Antony Vaillant <ghisvail@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from __future__ import division

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import ismrmrd
import ismrmrd_xsd
import pynfft


if __name__ == "__main__":
    argv = sys.argv[1:]
  
    if len(argv) == 0:
        print ('ismrmrd file is missing')
        sys.exit(2)
    else:
        filename = argv[0]
    
    if not os.path.isfile(filename):
        print("ismrmrd file does not exist" % filename)
        sys.exit(2)
    
    print("DEBUG: reading ISMRMRD header")
    dset = ismrmrd.IsmrmrdDataset(filename, 'dataset')
    header = ismrmrd_xsd.CreateFromDocument(dset.readHeader())
    enc = header.encoding[0]

    # Matrix size
    eNx = enc.encodedSpace.matrixSize.x
    eNy = enc.encodedSpace.matrixSize.y
    rNx = enc.reconSpace.matrixSize.x
    rNy = enc.reconSpace.matrixSize.y

    # Field of View
    eFOVx = enc.encodedSpace.fieldOfView_mm.x
    eFOVy = enc.encodedSpace.fieldOfView_mm.y
    rFOVx = enc.reconSpace.fieldOfView_mm.x
    rFOVy = enc.reconSpace.fieldOfView_mm.y

    # K-space encoding
    try:
        nkes0 = enc.encodingLimits.kspace_encoding_step_0.maximum + 1
    except AttributeError:
        nkes0 = eNx
    nkes1 = enc.encodingLimits.kspace_encoding_step_1.maximum + 1

    # Number of Slices, Reps, Contrasts, etc.
    #nslices = enc.encodingLimits.slice.maximum + 1
    ncoils = header.acquisitionSystemInformation.receiverChannels
    #nreps = enc.encodingLimits.repetition.maximum + 1
    #ncontrasts = enc.encodingLimits.contrast.maximum + 1

    ignore_list = []
    nacq = dset.getNumberOfAcquisitions()
    for iacq in range(nacq):
        acq = dset.readAcquisition(iacq)
        if acq.getHead().flags & ismrmrd.ACQ_IS_NOISE_MEASUREMENT:
            print("Found noise measurement @ %d" % iacq)
            ignore_list.append(iacq)

    print("DEBUG: reading ISMRMRD data")
    kdata = np.empty([ncoils, nkes1, nkes0], dtype=np.complex128)
    ktraj = np.empty([nkes1, nkes0, 2], dtype=np.float64)
    for iacq in range(nacq):
        if iacq in ignore_list:
            continue
        acq = dset.readAcquisition(iacq)
        ktraj[iacq] = acq.getTraj().reshape([nkes0, 2])
        kdata_iacq = acq.getData().reshape([ncoils, -1, 2])
        kdata_iacq_cpx = kdata_iacq[:, :, 0] + 1j * kdata_iacq[:, :, 1]
        kdata[:, iacq, :] = kdata_iacq_cpx.reshape([ncoils, nkes0])

    print("DEBUG: computing radial DCF")
    # FIXME: normalize DCF  
    dcf = np.abs(np.linspace(-0.5, 0.5, nkes0, endpoint=False))
    dcf = dcf.reshape([1, -1])

    print("DEBUG: instantiate plan")
    plan = pynfft.nfft.NFFT(N=[eNx, eNy], M=nkes1*nkes0)
    plan.x = ktraj
    plan.precompute()
    
    print("DEBUG: reconstruct coil images")
    imdata = np.empty([ncoils, eNx, eNy], dtype=np.complex)
    for icoil in range(ncoils):
        plan.f = kdata[icoil] * dcf
        imdata[icoil, :, :] = plan.adjoint()

    # remove oversampling in reconstructed FOV if necessary
    if eNx != rNx:
        i0 = np.floor((eNx - rNx) / 2)
        i1 = i0 + rNx
        imdata = imdata[:, i0:i1, i0:i1]

    # display reconstructed images for each coil
    # coil images are displayed on 2 lines
    fig = plt.figure()
    nrows = 2
    ncols = ncoils // 2 + ncoils % 2
    for icoil in range(ncoils):
        fig.add_subplot(nrows, ncols, 1+icoil, title="coil #%d"%(1+icoil))
        coil_img = np.abs(imdata[icoil])
        plt.imshow(coil_img, cmap='gray')
        plt.axis('off')
    fig.set_size_inches(16, 4)
    fig.show()

    raw_input("Press any key to exit...")
