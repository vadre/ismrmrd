#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

class imageReconBase
{
  public:
  virtual      ~imageReconBase() = default;
  virtual void addAcquisition (ISMRMRD::Entity* ent, uint32_t storage) = 0;
  virtual void addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr) = 0;
  virtual bool isImageDone () = 0;

  virtual ISMRMRD::Entity* getImageEntityPointer () = 0;
 };
