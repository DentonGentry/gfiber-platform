#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include <linux/sockios.h>
#include <linux/types.h>

static int skfd = -1;
static struct ifreq ifr;

struct mii_data {
  __u16 phy_id;
  __u16 reg_num;
  __u16 val_in;
  __u16 val_out;
};

int mdio_read(int location) {
  struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;
  mii->reg_num = location;
  if (ioctl(skfd, SIOCGMIIREG, &ifr) < 0) {
    fprintf(stderr, "SIOCGMIIREG on %s failed: %s\n", ifr.ifr_name,
            strerror(errno));
    return -1;
  }
  return mii->val_out;
}

int mdio_write(int location, int value) {
  struct mii_data *mii = (struct mii_data *)&ifr.ifr_data;
  mii->reg_num = location;
  mii->val_in = value;
  if (ioctl(skfd, SIOCSMIIREG, &ifr) < 0) {
    fprintf(stderr, "SIOCSMIIREG on %s failed: %s\n", ifr.ifr_name,
            strerror(errno));
    return -1;
  }
  return 0;
}

void mdio_init() {
  if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket init failed");
    exit(-1);
  }
}

int mdio_set_interface(const char *ifname) {
  strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
  if (ioctl(skfd, SIOCGMIIPHY, &ifr) < 0) {
    if (errno != ENODEV)
      fprintf(stderr, "SIOCGMIIPHY on '%s' failed: %s\n", ifname,
              strerror(errno));
    return -1;
  }
  return 0;
}

void mdio_done() { close(skfd); }
