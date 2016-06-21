#ifndef ICP_CALLBACK_H
#define ICP_CALLBACK_H
class icpCallback
{
  public:
  virtual          ~icpCallback() = default;
  virtual void     receive (icpCallback*         instance,
                            ISMRMRD::Entity*     entity,
                            uint32_t             version,
                            ISMRMRD::EntityType  etype,
                            ISMRMRD::StorageType stype,
                            uint32_t             stream) = 0;
};
#endif //ICP_CALLBACK_H
