#ifndef ICP_SERVERCALLBACKS_H
#define ICP_SERVERCALLBACKS_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include "icpServer.h"
#include "icpCallback.h"
#include "fftw3.h"

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
class icpServerImageRecon : public icpCallback
{
  public:
          icpServerImageRecon (icpServer*);
          ~icpServerImageRecon () = default;
  void    receive (icpCallback*, ENTITY*, uint32_t, ETYPE, STYPE, uint32_t);
  //void runReconstruction (icpServerImageRecon*);
  //template<typename S>
  //std::vector<ISMRMRD::Acquisition<S> > getAcquisitions ();
  template<typename S>
  void    reconstruct (icpServerImageRecon*);
  //void    reconstruct (std::vector<ISMRMRD::Acquisition<S> >& acqs);
  template<typename S>
  ISMRMRD::Entity* copyEntity (ISMRMRD::Entity* ent);
  //ISMRMRD::Entity* copyEntity (ISMRMRD::Entity* ent, uint32_t storage);

  private:

  icpServer*                     _server;
  ISMRMRD::IsmrmrdHeader         _hdr;
  bool                           _header_received;
  std::vector<ISMRMRD::Entity*>  _acqs;
  ISMRMRD::StorageType           _acq_storage;
  bool                           _acq_storage_set;

};

template <typename S>
ISMRMRD::Entity* icpServerImageRecon::icpServerImageRecon::copyEntity
(
  ISMRMRD::Entity* ent
)
{
  ISMRMRD::Acquisition<S>* tmp = static_cast<ISMRMRD::Acquisition<S>*>(ent);

  std::unique_ptr <ISMRMRD::Acquisition<S>>
    acq (new ISMRMRD::Acquisition<S> (tmp->getNumberOfSamples(),
                                      tmp->getActiveChannels(),
                                      tmp->getTrajectoryDimensions()));
  //ISMRMRD::Acquisition<S>* acq
    //(new ISMRMRD::Acquisition<S> (tmp->getNumberOfSamples(),
                                  //tmp->getActiveChannels(),
                                  //tmp->getTrajectoryDimensions()));
  acq->setHead (tmp->getHead());
  acq->setTraj (tmp->getTraj());
  acq->setData (tmp->getData());

  return &(*acq);
}
#endif //ICP_SERVERCALLBACKS_H
