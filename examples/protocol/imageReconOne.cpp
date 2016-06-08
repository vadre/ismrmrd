#include "imageReconOne.h"

ISMRMRD::Entity* imageReconOne::runReconstruction
(
  std::vector<ISMRMRD::Entity*>& ents,
  ISMRMRD::StorageType           storage,
  ISMRMRD::IsmrmrdHeader&        hdr
)
{
  if (storage == ISMRMRD::ISMRMRD_SHORT)
  {
    std::vector<ISMRMRD::Acquisition<int16_t> > acqs = getAcquisitions<int16_t> (ents);
    return reconstruct<int16_t> (acqs, hdr);
  }

  if (storage == ISMRMRD::ISMRMRD_INT)
  {
    std::vector<ISMRMRD::Acquisition<int32_t> > acqs = getAcquisitions<int32_t> (ents);
    return reconstruct<int32_t> (acqs, hdr);
  }

  if (storage == ISMRMRD::ISMRMRD_FLOAT)
  {
    std::vector<ISMRMRD::Acquisition<float> > acqs = getAcquisitions<float> (ents);
    return reconstruct<float> (acqs, hdr);
  }

  if (storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
    std::vector<ISMRMRD::Acquisition<double> > acqs = getAcquisitions<double> (ents);
    return reconstruct<double> (acqs, hdr);
  }

  throw std::runtime_error ("MR Acquisition unexpected storage type");

  return NULL;
}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
std::vector<ISMRMRD::Acquisition<S> > imageReconOne::getAcquisitions
(
  std::vector<ISMRMRD::Entity*>& ents
)
{
  std::vector<ISMRMRD::Acquisition<S> > acqs;

  for (int ii = 0; ii < ents.size(); ii++)
  {
    ISMRMRD::Acquisition<S>* tmp = static_cast<ISMRMRD::Acquisition<S>*>(ents[ii]);

    acqs.emplace (acqs.begin() + ii,
                  tmp->getNumberOfSamples(),
                  tmp->getActiveChannels(),
                  tmp->getTrajectoryDimensions());

    acqs[ii].setHead (tmp->getHead());
    acqs[ii].setTraj (tmp->getTraj());
    acqs[ii].setData (tmp->getData());
  }

  return acqs;
}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
ISMRMRD::Entity* imageReconOne::reconstruct
(
  std::vector<ISMRMRD::Acquisition<S> >& acqs,
  ISMRMRD::IsmrmrdHeader&                hdr
)
{
  std::cout << __func__ << " starting with " << acqs.size() << " acquisitions\n";

  ISMRMRD::EncodingSpace e_space = hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[0].reconSpace;

  if (e_space.matrixSize.z != 1)
  {
    throw std::runtime_error ("Only 2D encoding spaces supported");
  }

  uint16_t nX = e_space.matrixSize.x;
  uint16_t nY = e_space.matrixSize.y;

  uint32_t num_coils = acqs[0].getActiveChannels();

  std::vector<size_t> dims;
  dims.push_back (nX);
  dims.push_back (nY);
  dims.push_back (num_coils);
  ISMRMRD::NDArray<std::complex<S> > buffer (dims);

  unsigned int number_of_acquisitions = acqs.size();

  std::cout << "Encoding Matrix Size        : [" << e_space.matrixSize.x << ", "
            << e_space.matrixSize.y << ", " << e_space.matrixSize.z << "]" << std::endl;
  std::cout << "Reconstruction Matrix Size  : [" << r_space.matrixSize.x << ", "
            << r_space.matrixSize.y << ", " << r_space.matrixSize.z << "]" << std::endl;
  std::cout << "Number of Channels          : " << num_coils << std::endl;
  std::cout << "Number of acquisitions      : " << number_of_acquisitions << std::endl;



  for (unsigned int ii = 0; ii < number_of_acquisitions; ii++)
  {
    uint32_t ind = acqs[ii].getEncodingCounters().kspace_encode_step_1;
    for (uint16_t coil = 0; coil < num_coils; coil++)
    {
      memcpy (&buffer.at (0, ind, coil), &acqs[ii].at (0, coil),
              sizeof (std::complex<S>) * nX);
    }
  }

  for (uint16_t coil = 0; coil < num_coils; coil++)
  {
    fftwf_complex* tmp =
      (fftwf_complex*) fftwf_malloc (sizeof (fftwf_complex) * (nX * nY));

    if (!tmp)
    {
      throw std::runtime_error ("Error allocating storage for FFTW");
    }

    fftwf_plan p = fftwf_plan_dft_2d (nY, nX, tmp ,tmp, FFTW_BACKWARD, FFTW_ESTIMATE);

    fftshift (reinterpret_cast<std::complex<S>*>(tmp),
              &buffer.at (0, 0, coil), nX, nY);

    fftwf_execute(p);

    fftshift (&buffer.at (0, 0, coil),
              reinterpret_cast<std::complex<S>*>(tmp), nX, nY);

    fftwf_destroy_plan(p);
    fftwf_free(tmp);
  }

  ISMRMRD::Image<S>* img (new ISMRMRD::Image<S>(r_space.matrixSize.x,
                                                r_space.matrixSize.y, 1, 1));

  //If there is oversampling in the readout direction remove it
  uint16_t offset = ((e_space.matrixSize.x - r_space.matrixSize.x) / 2);
  for (uint16_t y = 0; y < r_space.matrixSize.y; y++)
  {
    for (uint16_t x = 0; x < r_space.matrixSize.x; x++)
    {
      for (uint16_t coil = 0; coil < num_coils; coil++)
      {
        img->at (x, y) += (std::abs (buffer.at (x + offset, y, coil))) *
                          (std::abs (buffer.at (x + offset, y, coil)));
      }
      img->at (x, y) = std::sqrt (img->at (x, y)); // Scale
    }
  }

  // The following are extra guidance we can put in the image header
  img->setStream (ISMRMRD::ISMRMRD_STREAM_IMAGE);
  img->setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
  img->setSlice (0);
  img->setFieldOfView (r_space.fieldOfView_mm.x,
                       r_space.fieldOfView_mm.y,
                       r_space.fieldOfView_mm.z);
  return img;
}
