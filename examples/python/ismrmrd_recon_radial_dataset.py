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

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import ismrmrd
import ismrmrd_xsd
import pynfft


def main(argv):
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

    ignore_list = []
    nacq = dset.getNumberOfAcquisitions()
    for iacq in range(nacq):
        acq = dset.readAcquisition(iacq)
        if acq.getHead().flags & ismrmrd.ACQ_IS_NOISE_MEASUREMENT:
            print("Found noise measurement @ %d" % iacq)
            ignore_list.append(iacq)

    print("DEBUG: reading ISMRMRD data")
    kdata = np.empty([eNy, eNx], dtype=np.complex128)
    ktraj = np.empty([eNy, eNx, 2], dtype=np.float64)
    for iacq in range(nacq):
        if iacq in ignore_list:
            continue
        acq = dset.readAcquisition(iacq)
        ktraj[iacq, :, 0] = acq.getTraj()[0::2]
        ktraj[iacq, :, 1] = acq.getTraj()[1::2]
        kdata[iacq, :] = acq.getData()[0::2] + 1j * acq.getData()[1::2]

    print("DEBUG: computing radial DCF")
    dcf = np.abs(np.linspace(-0.5, 0.5, eNx, endpoint=False))
    dcf = dcf.reshape([1, -1])

    print("DEBUG: performing reconstruction")
    plan = pynfft.nfft.NFFT(N=[rNx, rNy], M=eNx*eNy)
    plan.x = ktraj
    plan.precompute()
    plan.f = kdata * dcf
    idata = plan.adjoint().copy()

    # remove oversampling in frequency encoding direction if necessary
    if eNx != rNx:
        i0 = np.floor((eNx - rNx) / 2)
        i1 = i0 + rNx
        idata = idata[i0:i1, :]

    # display reconstructed image
    fig = plt.figure()
    fig.set_size_inches(4, 4)
    plt.imshow(np.abs(idata), cmap='gray')
    plt.show()



if __name__ == "__main__":
   main(sys.argv[1:])
