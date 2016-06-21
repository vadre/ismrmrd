#include <iostream>
#include "icpServerCallbacks.h"

/*******************************************************************************
 ******************************************************************************/
icpServerEntityHandler::icpServerEntityHandler
(
  icpServer* server
)
: _server (server)
{
}
/******************************************************************************/

void icpServerEntityHandler::receive
(
  icpCallback* base,
  ENTITY*      entity,
  uint32_t     version,
  ETYPE        etype,
  STYPE        stype,
  uint32_t     stream
)
{
  switch (etype)
  {
    case ISMRMRD::ISMRMRD_HANDSHAKE:

      _server->processHandshake (static_cast<HANDSHAKE*>(entity));
      break;

    case ISMRMRD::ISMRMRD_COMMAND:
      _server->processCommand (static_cast<COMMAND*>(entity));
      break;

    case ISMRMRD::ISMRMRD_ERROR_REPORT:

      _server->processError (static_cast<ERRREPORT*>(entity));
      break;

    default:

      std::cout << "Warning! Entity Handler received unexpected entity\n";
      break;
  }
}

/*******************************************************************************
 ******************************************************************************/
icpServerImageRecon::icpServerImageRecon
(
  icpServer* server
)
: _server (server)
{
}
/******************************************************************************/

void icpServerImageRecon::receive
(
  icpCallback* base,
  ENTITY*      entity,
  uint32_t     version,
  ETYPE        etype,
  STYPE        stype,
  uint32_t     stream
)
{
  icpServerImageRecon* _this = static_cast<icpServerImageRecon*>(base);

  if (etype == ISMRMRD::ISMRMRD_HEADER_WRAPPER)
  {
    _this->_hdr =
      (static_cast<ISMRMRD::IsmrmrdHeaderWrapper*>(entity))->getHeader();
    _this->_header_received = true;

    // For dataset testing
    //ISMRMRD::IsmrmrdHeaderWrapper wrapper;
    //wrapper.setHeader (_server->_hdr);
    //_server->_session->send (&wrapper, ISMRMRD::ISMRMRD_HEADER_WRAPPER);
    // Or:
    //_server->_session->send (entity, etype);
    std::cout << "Received ismrmrd header\n";
  }
  else if (etype == ISMRMRD::ISMRMRD_MRACQUISITION)
  {
    // TODO: Check stream number here
    if (!_this->_acq_storage_set)
    {
      _this->_acq_storage     = stype;
      _this->_acq_storage_set = true;
    }
    else if (_this->_acq_storage != stype)
    {
      throw std::runtime_error ("Inconsistent acquisition storage type");
    }

    uint32_t expected_num_acqs =
      _this->_hdr.encoding[0].encodedSpace.matrixSize.y;

    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      _this->_acqs.push_back (_this->copyEntity<int16_t> (entity));
      if (_this->_acqs.size() == expected_num_acqs)
      {
        _this->reconstruct<int16_t> (_this);
      }
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      _this->_acqs.push_back (_this->copyEntity<int32_t> (entity));
      if (_this->_acqs.size() == expected_num_acqs)
      {
        _this->reconstruct<int32_t> (_this);
      }
    }
    else if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      _this->_acqs.push_back (_this->copyEntity<float> (entity));
      if (_this->_acqs.size() == expected_num_acqs)
      {
        _this->reconstruct<float> (_this);
      }
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      _this->_acqs.push_back (_this->copyEntity<double> (entity));
      if (_this->_acqs.size() == expected_num_acqs)
      {
        _this->reconstruct<double> (_this);
      }
    }
  }
  else
  {
    std::cout << "Warning! Image Recon received unexpected entity\n";
  }
}

/******************************************************************************/
/*
void icpServerImageRecon::runReconstruction
(
  icpServerImageRecon* _this
)
{
  if (_this->_acq_storage == ISMRMRD::ISMRMRD_SHORT)
  {
    std::vector<ISMRMRD::Acquisition<int16_t> > acqs = _this->getAcquisitions<int16_t> ();
    _this->reconstruct (acqs);
  }

  if (_this->_acq_storage == ISMRMRD::ISMRMRD_INT)
  {
    std::vector<ISMRMRD::Acquisition<int32_t> > acqs = _this->getAcquisitions<int32_t> ();
    _this->reconstruct (acqs);
  }

  if (_this->_acq_storage == ISMRMRD::ISMRMRD_FLOAT)
  {
    std::vector<ISMRMRD::Acquisition<float> > acqs = _this->getAcquisitions<float> ();
    _this->reconstruct (acqs);
  }

  if (_this->_acq_storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
    std::vector<ISMRMRD::Acquisition<double> > acqs = _this->getAcquisitions<double> ();
    _this->reconstruct (acqs);
  }

  throw std::runtime_error ("MR Acquisition unexpected storage type");
}
*/
/******************************************************************************/
/*
template <typename S>
std::vector<ISMRMRD::Acquisition<S> > icpServerImageRecon::getAcquisitions
(
)
{
  std::vector<ISMRMRD::Acquisition<S> > acqs;

  for (int ii = 0; ii < _acqs.size(); ii++)
  {
    ISMRMRD::Acquisition<S>* tmp = static_cast<ISMRMRD::Acquisition<S>*>(_acqs[ii]);

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
*/
/******************************************************************************/
template <typename S>
void icpServerImageRecon::reconstruct
(
  icpServerImageRecon* _this
  //std::vector<ISMRMRD::Acquisition<S> >& acqs
)
{
  std::cout << __func__ << " starting with " << _this->_acqs.size() << " acquisitions\n";

  ISMRMRD::EncodingSpace e_space = _this->_hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = _this->_hdr.encoding[0].reconSpace;

  if (e_space.matrixSize.z != 1)
  {
    throw std::runtime_error ("Only 2D encoding spaces supported");
  }

  uint16_t nX = e_space.matrixSize.x;
  uint16_t nY = e_space.matrixSize.y;

  uint32_t num_coils =
    static_cast<ISMRMRD::Acquisition<S>*>(_this->_acqs[0])->getActiveChannels();

  std::vector<size_t> dims;
  dims.push_back (nX);
  dims.push_back (nY);
  dims.push_back (num_coils);
  ISMRMRD::NDArray<std::complex<S> > buffer (dims);

  unsigned int number_of_acquisitions = _acqs.size();

  std::cout << "Encoding Matrix Size        : [" << e_space.matrixSize.x << ", "
            << e_space.matrixSize.y << ", " << e_space.matrixSize.z << "]" << std::endl;
  std::cout << "Reconstruction Matrix Size  : [" << r_space.matrixSize.x << ", "
            << r_space.matrixSize.y << ", " << r_space.matrixSize.z << "]" << std::endl;
  std::cout << "Number of Channels          : " << num_coils << std::endl;
  std::cout << "Number of acquisitions      : " << number_of_acquisitions << std::endl;



  for (unsigned int ii = 0; ii < number_of_acquisitions; ii++)
  {
    uint32_t ind = static_cast<ISMRMRD::Acquisition<S>*>(_this->_acqs[ii])->
                     getEncodingCounters().kspace_encode_step_1;
    std::cout << "index = " << ind << "\n";

    for (uint16_t coil = 0; coil < num_coils; coil++)
    {

      //memcpy (&buffer.at (0, ind, coil), &acqs[ii].at (0, coil),
      ISMRMRD::Acquisition<S>* acq =
        static_cast<ISMRMRD::Acquisition<S>*>(_this->_acqs[ii]);
      std::cout << "Num samples = " << acq->getNumberOfSamples() << "\n";

      memcpy (&buffer.at (0, ind, coil), &acq->at (0, coil),
              sizeof (std::complex<S>) * nX);
    }
  }

  std::cout << __func__ << ": 2\n";
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

  std::cout << __func__ << ": 3\n";
  //std::unique_ptr<ISMRMRD::Image<S> >
    //img (new ISMRMRD::Image<S>(r_space.matrixSize.x, r_space.matrixSize.y, 1, 1));
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
        img->at(x,y) += (std::abs (buffer.at (x + offset, y, coil))) *
                        (std::abs (buffer.at (x + offset, y, coil)));
      }

      img->at (x, y) = std::sqrt (img->at (x, y)); // Scale
    }
  }

  std::cout << __func__ << ": 4\n";
  // The following are extra guidance we can put in the image header
  img->setStream (ISMRMRD_VERSION_MAJOR);
  img->setStream (ISMRMRD::ISMRMRD_STREAM_IMAGE); //TODO: ???
  img->setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
  img->setSlice (0);
  img->setFieldOfView (r_space.fieldOfView_mm.x,
                       r_space.fieldOfView_mm.y,
                       r_space.fieldOfView_mm.z);

  //std::shared_ptr<ISMRMRD::Image<S>*> img_entity (new ISMRMRD::Image<S>*(&(*img)));

  //std::cout << static_cast<ISMRMRD::Image<S>*>(img_entity)->getStream() << "\n";
  //std::cout << "storage = " << static_cast<ISMRMRD::Image<S>*>(img_entity)->getStorageType() << "\n";
  std::cout << "stream  = " << img->getStream() << "\n";
  std::cout << "storage = " << img->getStorageType() << "\n";
  std::cout << __func__ << ": img = " << img << ", done " << "\n";



  _server->sendImage (img, img->getVersion(), ISMRMRD::get_storage_type<S>(),
                      img->getStream());
  delete (img);
  _server->taskDone();
}

/*******************************************************************************
 ******************************************************************************/
