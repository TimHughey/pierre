#include "pierre/audiotx.hpp"

int main(int argc, char *argv[]) {
  pierre::string_t dest_host;

  if (argc > 1) {
    dest_host = argv[1];
  } else {
    dest_host = "test-with-devs.ruth.wisslanding.com";
  }

  pierre::AudioTx_t audio_tx(dest_host);

  if (audio_tx.init()) {
    audio_tx.run();
  }

  return 0;
}
