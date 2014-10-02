#include <virtio/class_virtio.hpp>
#include <syscalls.hpp>
#include <virtio/virtio.h>
#include <class_irq_handler.hpp>
#include <assert.h>

void Virtio::set_irq(){
  
  //Get device IRQ 
  uint32_t value = _pcidev.read_dword(PCI_CONFIG_INTR);
  if ((value & 0xFF) > 0 && (value & 0xFF) < 32){
    _irq = value & 0xFF;    
  }
  
}


Virtio::Virtio(PCI_Device* dev)
  : _pcidev(*dev), _virtio_device_id(dev->product_id() + 0x1040)
{
  printf("\n>>> Virtio attaching to  PCI addr 0x%x \n",_pcidev.pci_addr());
  
  /** PCI Device discovery. Virtio std. §4.1.2 */
  
  // Match vendor ID and Device ID : §4.1.2.2
  if (_pcidev.vendor_id() != PCI_Device::VENDOR_VIRTIO)
    panic("This is not a Virtio device");
  printf("\t [x] Vendor ID is VIRTIO \n");
  
  bool _STD_ID = _virtio_device_id >= 0x1040 and _virtio_device_id < 0x107f;
  bool _LEGACY_ID = _pcidev.product_id() >= 0x1000 
    and _pcidev.product_id() <= 0x103f;
  
  printf("\t [%s] Device ID 0x%x is in a valid range (%s)\n",
          _STD_ID or _LEGACY_ID ? "x" : " ",
         _pcidev.product_id(), _STD_ID ? ">= Virtio 1.0" : 
         (_LEGACY_ID ? "Virtio LEGACY" : "INVALID"));
    
  assert(_STD_ID or _LEGACY_ID);
  
  // Match Device revision ID. Virtio Std. §4.1.2.2
  bool rev_id_ok = ((_LEGACY_ID and _pcidev.rev_id() == 0) or
                    (_STD_ID and _pcidev.rev_id() > 0));
    
  
  printf("\t [%s] Device Revision ID (0x%x) supported. \n",
         rev_id_ok and version_supported(_pcidev.rev_id()) ? "x" 
         : " ",_pcidev.rev_id());
  
  assert(rev_id_ok); // We'll try to continue if it's newer than supported.
  
  //Fetch IRQ from PCI resource
  set_irq();
  printf(_irq ? "\t [x] Unit IRQ %i \n " : "\n [ ] NO IRQ on device \n",_irq);

  _pcidev.probe_resources();
  _iobase=_pcidev.iobase();

  printf(_irq ? "\t [x] Unit I/O base 0x%lx \n " : 
         "\n [ ] NO I/O Base on device \n",_iobase);


  //@note this is "the Legacy interface" according to Virtio std. 4.1.4.8. 
  uint32_t queue_size = inpd(_iobase + 0x0C);
  
  printf(queue_size > 0 and queue_size != PCI_WTF ?
         "\t [x] Queue Size : 0x%lx \n" :
         "\t [ ] No qeuue Size? : 0x%lx \n" ,queue_size);


  // Do stuff in the order described in Virtio standard v.1, sect. 3.1:
  // ...Points 1-6. 
  
  // 1. Reset device
  reset();
  printf("\t [*] Reset device \n");
  
  // 2. Set driver status bit
  // TODO
  
  negotiate_features(0);
  printf("\t [*] Negotiate features \n");
  enable_irq_handler();
  printf("\t [*] Enable IRQ Handler \n");
  
  printf("\n  >> Virtio initialization complete \n\n");
  
  
  //OSdev lists this as a status field, but Ringaard does not.
  //uint32_t status1 = inpd(_iobase + 0x12);
  
}

void Virtio::get_config(void* buf, int len){
  unsigned char* ptr = (unsigned char*)buf;
  uint32_t ioaddr = _iobase + VIRTIO_PCI_CONFIG;
  int i;
  for (i = 0; i < len; i++) *ptr++ = inp(ioaddr + i);
}


void Virtio::reset(){
  outp(_iobase + VIRTIO_PCI_STATUS, 0);
}

void Virtio::sig_driver_found(){
  outp(_iobase + VIRTIO_PCI_STATUS, inp(_iobase + VIRTIO_PCI_STATUS) | VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER);

}


uint32_t Virtio::probe_features(){
  return inpd(_iobase + VIRTIO_PCI_HOST_FEATURES);
}

void Virtio::negotiate_features(uint32_t features){
  _features = inpd(_iobase + VIRTIO_PCI_HOST_FEATURES);
  _features &= features;
  outpd(_iobase + VIRTIO_PCI_GUEST_FEATURES, _features);
  _features = probe_features();
}






void Virtio::default_irq_handler(){
  printf("PRIVATE virtio IRQ handler: Call %i \n",calls++);
  printf("Old Features : 0x%lx \n",_features);
  printf("New Features : 0x%lx \n",probe_features());
  
  unsigned char isr = inp(_iobase + VIRTIO_PCI_ISR);
  printf("Virtio ISR: 0x%i \n",isr);
  printf("Virtio ISR: 0x%i \n",isr);
  
  eoi(_irq);
  
}



void Virtio::enable_irq_handler(){
  //_irq=0; //Works only if IRQ2INTR(_irq), since 0 overlaps an exception.

  
  auto del=delegate::from_method<Virtio,&Virtio::default_irq_handler>(this);  
  
  IRQ_handler::subscribe(_irq,del);
  
  IRQ_handler::enable_irq(_irq);
  
}

/** void Virtio::enable_irq_handler(IRQ_handler::irq_delegate d){
  //_irq=0; //Works only if IRQ2INTR(_irq), since 0 overlaps an exception.
  //IRQ_handler::set_handler(IRQ2INTR(_irq), irq_virtio_entry);
  
  IRQ_handler::subscribe(_irq,d);
  
  IRQ_handler::enable_irq(_irq);
  }*/




