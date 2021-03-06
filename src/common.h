#define LIBSUPERDERPY_DATA_TYPE struct CommonResources
#include <libsuperderpy.h>

struct CommonResources {
	// Fill in with common data accessible from all gamestates.
	bool unused;
};

float* CreateHanningWindow(int N, bool periodic);
struct CommonResources* CreateGameData(struct Game* game);
void DestroyGameData(struct Game* game);
bool GlobalEventHandler(struct Game* game, ALLEGRO_EVENT* event);
