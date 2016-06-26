
unsigned int crc32(unsigned int crc,unsigned char *buf,unsigned int len)
{
    static unsigned int crc_table[256];
    if (!crc_table[1])
    {
        unsigned int c;
        int n, k;
        for (n = 0; n < 256; n++)
        {
            c = (unsigned int)n;
            for (k=0; k<8; k++) c=(c >> 1) ^ (c & 1 ? 0xedb88320L : 0);
            crc_table[n]=c;
        }
    }
    crc = crc ^ 0xffffffffL;
    while (len-- > 0){
            crc = crc_table[(crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffffL;
}
