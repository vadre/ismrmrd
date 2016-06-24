#ifndef ICP_SERVERCALLBACKS_H
#define ICP_SERVERCALLBACKS_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include "icpServer.h"
#include "icpCallback.h"
#include "fftw3.h"

/*******************************************************************************
 class icpServerEntityHandler, implementation is in icpServerCallbacks.cpp
 ******************************************************************************/
class icpServerEntityHandler : public icpCallback
{
  public:
          icpServerEntityHandler (icpServer*);
          ~icpServerEntityHandler () = default;
  void    receive (icpCallback*, ENTITY*, uint32_t, ETYPE, STYPE, uint32_t);

  private:

  icpServer* _server;
};

/*******************************************************************************
 Class template icpServerImageRecon and implementation below -
 ******************************************************************************/
template <typename T>
void circshift(T *out, const T *in, int xdim, int ydim, int xshift, int yshift)
{
  for (int i =0; i < ydim; i++)
  {
    int ii = (i + yshift) % ydim;
    for (int j = 0; j < xdim; j++)
    {
      int jj = (j + xshift) % xdim;
      out[ii * xdim + jj] = in[i * xdim + j];
    }
  }
}

#define fftshift(out, in, x, y) circshift(out, in, x, y, (x/2), (y/2))

/******************************************************************************/
template <typename S>
class icpServerImageRecon : public icpCallback
{
  public:

  icpServerImageRecon
  (
    icpServer* server
  )
  : _server            (server),
    _header_received   (false),
    _stop_acqs         (false),
    _expected_num_acqs (0),
    _params_set        (false),
    _num_coils         (0)
  {}

  ~icpServerImageRecon () = default;

/******************************************************************************/
  void receive
  (
    icpCallback* base,
    ENTITY*      entity,
    uint32_t     version,
    ETYPE        etype,
    STYPE        stype,
    uint32_t     stream
  )
  {
    icpServerImageRecon<S>* _this = static_cast<icpServerImageRecon<S>*>(base);

    if (etype == ISMRMRD::ISMRMRD_HEADER)
    {
      _this->processHeader (entity);
    }
    else if (etype == ISMRMRD::ISMRMRD_MRACQUISITION)
    {
      if (_this->_stop_acqs)
      {
        std::cout << "Warning! Image Recon received extra acquisition\n";
        return;
      }
      _this->processAcquisition (entity, version, stype, stream);
    }
    else
    {
      std::cout << "Warning! Image Recon received unexpected entity\n";
    }
  }

/******************************************************************************/
  private:

  icpServer*                           _server;
  ISMRMRD::IsmrmrdHeader               _header;
  bool                                 _header_received;
  bool                                 _stop_acqs;
  uint32_t                             _expected_num_acqs;
  bool                                 _params_set;
  uint32_t                             _version;
  uint32_t                             _stream;
  uint32_t                             _num_coils;
  ISMRMRD::StorageType                 _stype;
  std::queue<ISMRMRD::Acquisition<S>>  _acqs;

/******************************************************************************/
  void processHeader
  (
    ENTITY* entity
  )
  {
    ISMRMRD::IsmrmrdHeaderWrapper* wrp =
        dynamic_cast<ISMRMRD::IsmrmrdHeaderWrapper*>(entity);
    std::string hdr_str = wrp->getString();
    _header = wrp->getHeader();
    _expected_num_acqs = _header.encoding[0].encodedSpace.matrixSize.y;
    _header_received = true;
  
    std::cout << "Received ismrmrd header\n";
  
    //_server->sendHeader (entity, ISMRMRD::ISMRMRD_HEADER);
  }

/******************************************************************************/
  void processAcquisition
  (
    ENTITY*                 entity,
    uint32_t                version,
    STYPE                   stype,
    uint32_t                stream
  )
  {
    ISMRMRD::Acquisition<S>* a = static_cast<ISMRMRD::Acquisition<S>*>(entity);

    if (_params_set)
    {
      if (_version   != version  ||
          _stype     != stype    ||
          _stream    != stream   ||
          _num_coils != a->getActiveChannels())
      {
        std::cout << "Warning! inconsistent acquisition header parameters\n";
        //TODO: _server->sendError();
        return;
      }
    }
    else
    {
      _version    = version;
      _stype      = stype;
      _stream     = stream;
      _num_coils  = a->getActiveChannels();
      _params_set = true;
    }

    _acqs.push (*a);
    if (_acqs.size() == _expected_num_acqs)
    {
      reconstruct ();
    }
  }

/******************************************************************************/
  void reconstruct
  (
  )
  {
    unsigned int num_acqs = _acqs.size();
    std::cout << __func__ << " starting with " << num_acqs << " acquisitions\n";

    ISMRMRD::EncodingSpace e_space = _header.encoding[0].encodedSpace;
    ISMRMRD::EncodingSpace r_space = _header.encoding[0].reconSpace;

    if (e_space.matrixSize.z != 1)
    {
      throw std::runtime_error ("Only 2D encoding spaces supported");
    }

    uint16_t nX = e_space.matrixSize.x;
    uint16_t nY = e_space.matrixSize.y;

    std::vector<size_t> dims;
    dims.push_back (nX);
    dims.push_back (nY);
    dims.push_back (_num_coils);
    ISMRMRD::NDArray<std::complex<S> > buffer (dims);

    for (unsigned int ii = 0; ii < num_acqs; ii++)
    {
      ISMRMRD::Acquisition<S> acq = _acqs.front ();
      _acqs.pop ();
      uint32_t ind = acq.getEncodingCounters().kspace_encode_step_1;

      for (uint16_t coil = 0; coil < _num_coils; coil++)
      {
        memcpy (&buffer.at (0, ind, coil),
                &acq.at (0, coil),
                sizeof (std::complex<S>) * nX);
      }
    }

    for (uint16_t coil = 0; coil < _num_coils; coil++)
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

    ISMRMRD::Image<S> img (r_space.matrixSize.x, r_space.matrixSize.y, 1, 1);

    //If there is oversampling in the readout direction remove it
    uint16_t offset = ((e_space.matrixSize.x - r_space.matrixSize.x) / 2);

    for (uint16_t y = 0; y < r_space.matrixSize.y; y++)
    {
      for (uint16_t x = 0; x < r_space.matrixSize.x; x++)
      {
        for (uint16_t coil = 0; coil < _num_coils; coil++)
        {
          img.at (x, y) += (std::abs (buffer.at (x + offset, y, coil))) *
                            (std::abs (buffer.at (x + offset, y, coil)));
        }

        img.at (x, y) = std::sqrt (img.at (x, y)); // Scale
      }
    }

    // The following are extra guidance we can put in the image header
    img.setStream (ISMRMRD_VERSION_MAJOR);
    img.setStream (ISMRMRD::ISMRMRD_STREAM_IMAGE); //TODO: ???
    img.setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
    img.setSlice (0);
    img.setFieldOfView (r_space.fieldOfView_mm.x,
                         r_space.fieldOfView_mm.y,
                         r_space.fieldOfView_mm.z);

    _server->sendImage (&img, img.getVersion(),
                        img.getStorageType(),
                        img.getStream());

    _server->taskDone();
    std::cout << "Image reconstruction done\n";
  }

/******************************************************************************/
};

/*******************************************************************************
 ******************************************************************************/

#endif //ICP_SERVERCALLBACKS_H
