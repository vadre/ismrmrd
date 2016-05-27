#include "imageReconOne.h"

/*******************************************************************************
 ******************************************************************************/
imageReconOne::imageReconOne()
: _header_received (false),
  _acq_storage_set (false),
  _image_done      (false)
{
}

/*******************************************************************************
 ******************************************************************************/
imageReconOne::~imageReconOne()
{
  _acqs.clear();
}

/*******************************************************************************
 ******************************************************************************/
ISMRMRD::Entity* imageReconOne::getImageEntityPointer
(
)
{
  return _img_entity;
}

/*******************************************************************************
 ******************************************************************************/
bool imageReconOne::isImageDone
(
)
{
  return _image_done;
}

/*******************************************************************************
 ******************************************************************************/
void imageReconOne::addIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader hdr
)
{
  _header = hdr;
  _header_received = true;
}

/*******************************************************************************
 ******************************************************************************/
void imageReconOne::addAcquisition
(
  ISMRMRD::Entity* ent,
  uint32_t         storage
)
{
  if (!_acq_storage_set)
  {
    _acq_storage     = storage;
    _acq_storage_set = true;
  }
  else if (storage != _acq_storage)
  {
    throw std::runtime_error ("MR Acquisition inconsistent storage type");
  }

  if (_acq_storage == ISMRMRD::ISMRMRD_FLOAT)
  {
    storeAcquisition (static_cast<ISMRMRD::Acquisition<float>*>(ent));
    if (_acqs.size() == _header.encoding[0].encodedSpace.matrixSize.y)
    {
      runReconstruction<float>();
      _image_storage = ISMRMRD::ISMRMRD_FLOAT;
    }
  }
  else if (_acq_storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
    storeAcquisition(static_cast<ISMRMRD::Acquisition<double>*>(ent));
    if (_acqs.size() == _header.encoding[0].encodedSpace.matrixSize.y)
    {
      runReconstruction<double>();
      _image_storage = ISMRMRD::ISMRMRD_DOUBLE;
    }
  }
  else
  {
    throw std::runtime_error ("MR Acquisition unexpected storage type");
  }

}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
void imageReconOne::storeAcquisition
(
  ISMRMRD::Acquisition<S>* acq
)
{
  std::unique_ptr<ISMRMRD::Acquisition<S>>
    tmp (new ISMRMRD::Acquisition<S> (acq->getNumberOfSamples(),
                                      acq->getActiveChannels(),
                                      acq->getTrajectoryDimensions()));
  tmp->setHead (acq->getHead());
  tmp->setTraj (acq->getTraj());
  tmp->setData (acq->getData());

  _acqs.push_back ((ISMRMRD::Entity*)&(*tmp));
}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
void imageReconOne::imageReconOne::runReconstruction ()
{
  std::cout << __func__ << " starting with " << _acqs.size() << " acquisitions\n";

  ISMRMRD::EncodingSpace e_space = _header.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = _header.encoding[0].reconSpace;

  if (e_space.matrixSize.z != 1)
  {
    throw std::runtime_error ("Only 2D encoding spaces supported");
  }

  uint16_t nX = e_space.matrixSize.x;
  uint16_t nY = e_space.matrixSize.y;

  uint32_t num_coils =
    static_cast<ISMRMRD::Acquisition<S>*>(_acqs[0])->getActiveChannels();

  std::vector<size_t> dims;
  dims.push_back (nX);
  dims.push_back (nY);
  dims.push_back (num_coils);
  ISMRMRD::NDArray<std::complex<S> > buffer (dims);

  unsigned int number_of_acquisitions = _acqs.size();

  for (unsigned int ii = 0; ii < number_of_acquisitions; ii++)
  {
    ISMRMRD::Acquisition<S>* tmp = static_cast<ISMRMRD::Acquisition<S>*>(_acqs[ii]);
    uint32_t ind = static_cast<ISMRMRD::Acquisition<S>*>(_acqs[ii])->
                     getEncodingCounters().kspace_encode_step_1;
    for (uint16_t coil = 0; coil < num_coils; coil++)
    {
      memcpy (&buffer.at (0, ind, coil), &tmp->at (0, coil),
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

  std::unique_ptr<ISMRMRD::Image<S>>
    img (new ISMRMRD::Image<S>(r_space.matrixSize.x, r_space.matrixSize.y, 1, 1));

  //If there is oversampling in the readout direction remove it
  uint16_t offset = ((e_space.matrixSize.x - r_space.matrixSize.x) / 2);
  for (uint16_t y = 0; y < r_space.matrixSize.y; y++)
  {
    for (uint16_t x = 0; x < r_space.matrixSize.x; x++)
    {
      for (uint16_t coil = 0; coil < num_coils; coil++)
      {
        img->at(x,y) += (std::abs (buffer.at (x + offset, y, coil))) *
                        (std::abs (buffer.at (x + offset, y, coil)));
      }

      img->at (x, y) = std::sqrt (img->at (x, y)); // Scale
    }
  }

  // The following are extra guidance we can put in the image header
  img->setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
  img->setSlice (0);
  img->setFieldOfView (r_space.fieldOfView_mm.x,
                       r_space.fieldOfView_mm.y,
                       r_space.fieldOfView_mm.z);

  _img_entity = (ISMRMRD::Entity*)&(*img);
  _image_done = true;

  std::cout << __func__ << " done " << "\n";

  return;
}
