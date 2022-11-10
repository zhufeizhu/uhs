#ifndef MIX_MOCK_SSD_H
#define MIX_MOCK_SSD_H

int mix_mock_ssd_init();

unsigned int mix_mock_ssd_read(void* dst,
                               unsigned int len,
                               unsigned int offset);

unsigned int mix_mock_ssd_write(void* src,
                                unsigned int len,
                                unsigned int offset);

#endif  // MIX_MOCK_SSD_H