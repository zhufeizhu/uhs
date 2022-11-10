#ifndef MIX_MOCK_NVM_H
#define MIX_MOCK_NVM_H

int mix_mock_nvm_init();

unsigned int mix_mock_nvm_read(void* dst,
                               unsigned int len,
                               unsigned int offset);

unsigned int mix_mock_nvm_write(void* src,
                                unsigned int len,
                                unsigned int offset);

#endif  // MIX_MOCK_NVM_H