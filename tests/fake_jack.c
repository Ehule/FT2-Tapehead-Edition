#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t jack_nframes_t;
typedef uint32_t jack_options_t;
typedef uint32_t jack_status_t;

typedef struct _jack_port
{
	char name[32];
	float buffer[256];
} jack_port_t;

typedef struct _jack_client
{
	int (*processCallback)(jack_nframes_t, void *);
	void *processUserdata;
	uint8_t portCount;
	jack_port_t ports[32];
} jack_client_t;

static jack_client_t fakeClient;
static uint32_t fakeActivateCount;
static uint32_t fakeDeactivateCount;
static int fakeClientActive;

jack_client_t *jack_client_open(const char *name, jack_options_t options,
	jack_status_t *status, ...)
{
	(void)name;
	(void)options;
	memset(&fakeClient, 0, sizeof (fakeClient));
	fakeActivateCount = 0;
	fakeDeactivateCount = 0;
	fakeClientActive = 0;
	if (status != NULL)
		*status = 0;
	return &fakeClient;
}

int jack_client_close(jack_client_t *client)
{
	(void)client;
	return 0;
}

int jack_activate(jack_client_t *client)
{
	fakeActivateCount++;
	fakeClientActive = 1;
	if (client->processCallback != NULL)
		return client->processCallback(64, client->processUserdata);
	return 0;
}

int jack_deactivate(jack_client_t *client)
{
	(void)client;
	fakeDeactivateCount++;
	fakeClientActive = 0;
	return 0;
}

int jack_set_process_callback(jack_client_t *client,
	int (*callback)(jack_nframes_t, void *), void *userdata)
{
	client->processCallback = callback;
	client->processUserdata = userdata;
	return 0;
}

jack_port_t *jack_port_register(jack_client_t *client, const char *name,
	const char *type, unsigned long flags, unsigned long bufferSize)
{
	(void)type;
	(void)flags;
	(void)bufferSize;
	if (client->portCount >= 32)
		return NULL;

	jack_port_t *port = &client->ports[client->portCount++];
	snprintf(port->name, sizeof (port->name), "%s", name);
	return port;
}

void *jack_port_get_buffer(jack_port_t *port, jack_nframes_t sampleFrames)
{
	(void)sampleFrames;
	return port->buffer;
}

jack_nframes_t jack_get_sample_rate(jack_client_t *client)
{
	(void)client;
	return 48000;
}

jack_nframes_t jack_get_buffer_size(jack_client_t *client)
{
	(void)client;
	return 64;
}

uint8_t fake_jack_port_count(void)
{
	return fakeClient.portCount;
}

const char *fake_jack_port_name(uint8_t portIndex)
{
	return portIndex < fakeClient.portCount ? fakeClient.ports[portIndex].name : NULL;
}

float fake_jack_sample(uint8_t portIndex, uint32_t sampleIndex)
{
	if (portIndex >= fakeClient.portCount || sampleIndex >= 256)
		return 0.0f;
	return fakeClient.ports[portIndex].buffer[sampleIndex];
}

uint32_t fake_jack_activate_count(void)
{
	return fakeActivateCount;
}

uint32_t fake_jack_deactivate_count(void)
{
	return fakeDeactivateCount;
}

int fake_jack_run_process(void)
{
	if (!fakeClientActive || fakeClient.processCallback == NULL)
		return -1;

	return fakeClient.processCallback(64, fakeClient.processUserdata);
}
