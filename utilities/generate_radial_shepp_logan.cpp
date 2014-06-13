/*
 * generate_radial_shepp_logan.cpp
 * 
 * Copyright 2014 Ghislain Antony Vaillant <ghisvail@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include <iostream>
#include <valarray>
#include <cmath>
#include <boost/program_options.hpp>
#include "ismrmrd_phantom.h"
#include "ismrmrd_hdf5.h"
#include "ismrmrd_nfft.h"
#include "ismrmrd.hxx"

using namespace ISMRMRD;
namespace po = boost::program_options;

int main(int argc, char** argv)
{
  unsigned int ndim = 2;  // 2D trajectory
  unsigned int matrix_size;
  unsigned int ros;
  std::string outfile;
  std::string dataset;
  unsigned int profiles;

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("matrix,m", po::value<unsigned int>(&matrix_size)->default_value(256), "Matrix size")
    ("oversampling,O", po::value<unsigned int>(&ros)->default_value(2), "Readout oversampling")
    ("output,o", po::value<std::string>(&outfile)->default_value("testdata.h5"), "Output file name")
    ("dataset,d", po::value<std::string>(&dataset)->default_value("dataset"), "Output dataset name")
    ("profiles,p", po::value<unsigned int>(&profiles)->default_value(256), "Number of angular profiles")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
      std::cout << desc << "\n";
      return 1;
  }

  /* some convenient geometry vars */
  unsigned int os_matrix_size = ros * matrix_size;
  unsigned int readout = ros * matrix_size;
  std::vector<unsigned int> imdims;
  imdims.push_back(os_matrix_size);
  imdims.push_back(os_matrix_size);
  std::vector<unsigned int> ksdims;
  ksdims.push_back(readout);
  ksdims.push_back(profiles);


  std::cout << "Generating radial Shepp-Logan phantom" << std::endl;

  std::cout << "DEBUG: Generating phantom data" << std::endl;
  boost::shared_ptr<NDArrayContainer<std::complex<float> > > phantom = shepp_logan_phantom(matrix_size);
  NDArrayContainer<std::complex<float> > os_phantom(imdims);
  os_phantom.data_ = std::complex<float>(0.0,0.0);
  size_t iindex, oindex;
  for (unsigned int y = 0; y < os_matrix_size; y++) {
    for (unsigned int x = 0; x < os_matrix_size; x++) {
      oindex = (y + ((os_matrix_size - matrix_size) >> 1)) * os_matrix_size +
          x + ((os_matrix_size - matrix_size) >> 1);
      iindex = y * matrix_size + x;
      os_phantom.data_[oindex] = phantom->data_[iindex];
    }
  }

  /* compute fully-sampled trajectory */
  std::cout << "DEBUG: Generating trajectory" << std::endl;
  std::valarray<float> kr(readout);
  for (unsigned int ife = 0; ife < readout; ife++)
    kr[ife] = (float)ife * (1.0f / readout) - 0.5f;

  std::valarray<float> full_traj(profiles*readout*ndim);
  std::slice ix_kx, ix_ky;
  float angle_ipe, cos_angle, sin_angle;
  for (unsigned int ipe = 0; ipe < profiles; ipe++) {
    angle_ipe = (float)ipe * (M_PI / profiles);
    cos_angle = cos(angle_ipe);
    sin_angle = sin(angle_ipe);
    ix_kx = std::slice(ipe*readout*ndim, readout, ndim);
    ix_ky = std::slice(ipe*readout*ndim+1, readout, ndim);    
    full_traj[ix_kx] = kr * cos_angle;
    full_traj[ix_ky] = kr * sin_angle;
  }

  /* instantiate the NFFT plan */
  std::cout << "DEBUG: instantiate plan" << std::endl;
  NFFT2 plan = NFFT2(os_matrix_size, os_matrix_size, profiles*readout);
  plan.precompute(full_traj);

  /* compute fully sampled k-space for each coil */
  NDArrayContainer<std::complex<float> > imdata = os_phantom;
  NDArrayContainer<std::complex<float> > ksdata(ksdims);

  /* itok */
  std::valarray<std::complex<float> > imdata_ico = imdata.data_;
  std::valarray<std::complex<float> > ksdata_ico = ksdata.data_;
  plan.trafo(imdata_ico, ksdata_ico);

  // create dataset
  IsmrmrdDataset d(outfile.c_str(), dataset.c_str());
  Acquisition acq;
  acq.setActiveChannels(1);
  acq.setAvailableChannels(1);
  acq.setNumberOfSamples(readout);
  acq.setCenterSample(readout>>1);

  /* record acquisiton of each phase encoding line individually */
  std::cout << "DEBUG: Looping through each encoding line" << std::endl;
  for (unsigned int ipe = 0; ipe < profiles; ipe++) {
    acq.setFlags(0);  // reset acquisition flags
    acq.setNumberOfSamples(readout);
    acq.setSampleTimeUs(5.0);  // arbitrary

    /* set relevant acquisition flags:
     * ACQ_FIRST_IN_SLICE for first profile
     * ACQ_LAST_IN_SLICE for last profile
     */
    if (ipe == 0)
      acq.setFlag(ISMRMRD::FlagBit(ISMRMRD::ACQ_FIRST_IN_SLICE));
    if (ipe == profiles-1)
      acq.setFlag(ISMRMRD::FlagBit(ISMRMRD::ACQ_LAST_IN_SLICE));

    /* set encoding:
     * limited to kspace_encode_step_1 in 2D 
     */
    acq.getIdx().kspace_encode_step_1 = ipe;

    /* set data:
     * copy the relevant portion of the fully-sampled data to the current 
     * acquisition instance
     */
    std::valarray<float> data(readout*2);
    std::valarray<std::complex<float> > ksdata_ico_ipe(readout);
    std::slice ix_ksdata_ipe(ipe*readout, readout, 1);
    ksdata_ico_ipe = ksdata_ico[ix_ksdata_ipe];
    memcpy(&data, &ksdata_ico_ipe, data.size()*sizeof(float))
    acq.setData(data);
 
    /* set trajectory:
     * copy the relevant portion of the fully-sampled trajectory
     */
    std::valarray<float> traj(readout*ndim);
    std::slice ix_this_kx(0, readout, ndim);
    std::slice ix_full_kx(ipe*readout*ndim, readout, ndim);
    std::slice ix_this_ky(1, readout, ndim);
    std::slice ix_full_ky(1+ipe*readout*ndim, readout, ndim);
    traj[ix_this_kx] = full_traj[ix_full_kx];
    traj[ix_this_ky] = full_traj[ix_full_ky];
    acq.setTrajectoryDimensions(2);
    acq.setTraj(traj);
    
    /* add current acquisition to dataset */
    d.appendAcquisition(&acq);
  }

  std::cout << "DEBUG: generating XML header" << std::endl;
  /* create XML header */
  ISMRMRD::experimentalConditionsType exp(63500000); //~1.5T
  ISMRMRD::acquisitionSystemInformationType sys;
  sys.institutionName("ISMRM Synthetic Imaging Lab");
  sys.receiverChannels(1);

  ISMRMRD::ismrmrdHeader h(exp);
  h.acquisitionSystemInformation(sys);

  /* create an encoding section */
  ISMRMRD::encodingSpaceType es(
      ISMRMRD::matrixSize(os_matrix_size, os_matrix_size, 1),
      ISMRMRD::fieldOfView_mm(ros*300, ros*300, 6)
      );
  ISMRMRD::encodingSpaceType rs(
      ISMRMRD::matrixSize(matrix_size, matrix_size, 1),
      ISMRMRD::fieldOfView_mm(300, 300, 6)
      );
  ISMRMRD::encodingLimitsType el;
  el.kspace_encoding_step_1(ISMRMRD::limitType(0, profiles-1, 0));
  ISMRMRD::encoding e(es, rs, el, ISMRMRD::trajectoryType::radial);

  /* add the encoding section to the header */
  h.encoding().push_back(e);

  /* serialize the header */
  xml_schema::namespace_infomap map;
  map[""].name = "http://www.ismrm.org/ISMRMRD";
  map[""].schema = "ismrmrd.xsd";
  std::stringstream str;
  ISMRMRD::ismrmrdHeader_(str, h, map);
  std::string xml_header = str.str();

  /* write the header to the data file. */
  d.writeHeader(xml_header);

  return 0;
}





