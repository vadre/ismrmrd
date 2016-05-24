#include "imageReconBase.h"

/*******************************************************************************
 ******************************************************************************/
void imageReconBase::addIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader hdr
)
{
  _header = hdr;
  _header_received = true;
}

/*******************************************************************************
 ******************************************************************************/
void imageReconBase::addAcquisition
(
  ISMRMRD::Entity* ent
)
{
  uint32_t storage =
    static_cast<ISMRMRD::Acquisition<int16_t>*>(ent).getHead().getStorageType();

  if (!_storage_set)
  {
    _storage = storage;
    _storage_set = true;
  }
  else if (storage != _storage)
  {
    std::cout << __func__ << ": Error - inconsistent storage type\n";
    return;
  }

  if (_storage == ISMRMRD::ISMRMRD_SHORT)
  {
    ISMRMRD::Acquisition<int16_t> acq =
      *static_cast<ISMRMRD::Acquisition<int16_t>*>(ent);

    _acq16.push_back (acq);
  }
}


