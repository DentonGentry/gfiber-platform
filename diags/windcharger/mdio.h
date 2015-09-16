#ifndef VENDOR_GOOGLE_DIAGS_SPACECAST_MDIO_H_
#define VENDOR_GOOGLE_DIAGS_SPACECAST_MDIO_H_

// In order to read/write PHY registers, it needs to do it in the
// following sequence:
// mdio_init
// mdio_set_interface
// mdio_read and/or mdio_write (as many as required)
// mdio_done

int mdio_read(int location);
int mdio_write(int location, int value);
void mdio_init();
int mdio_set_interface(const char* ifname);
void mdio_done();

#endif  // VENDOR_GOOGLE_DIAGS_SPACECAST_MDIO_H_
