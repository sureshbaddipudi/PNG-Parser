
void make_crc_table(void);
unsigned long update_crc(unsigned long crc, const unsigned char *buf, int len);
unsigned long crc(const unsigned char *buf, int len);

