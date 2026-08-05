#include <assert.h>
#include <stdio.h>
#include "ft2_sample_launcher_state.h"

static sampleLauncherState_t state;
static sampleLauncherAction_t actions[SAMPLE_LAUNCHER_MAX_ACTIONS];

static uint8_t commit(void)
{
	return sampleLauncherStateCommitBoundary(&state, actions);
}

static void test_q_replaces_only_at_boundary(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 3));
	assert(state.qCurrent == -1);
	assert(commit() == 1);
	assert(actions[0].type == SAMPLE_LAUNCHER_ACTION_START_Q);
	assert(state.qCurrent == 3);

	assert(sampleLauncherStateRequestQ(&state, 7));
	assert(state.qCurrent == 3);
	assert(commit() == 2);
	assert(actions[0].type == SAMPLE_LAUNCHER_ACTION_STOP_Q);
	assert(actions[1].type == SAMPLE_LAUNCHER_ACTION_START_Q);
	assert(state.qCurrent == 7);
}

static void test_q_reclick_gracefully_stops(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 2));
	commit();
	assert(sampleLauncherStateRequestQ(&state, 2));
	assert(state.qStopPending);
	assert(commit() == 1);
	assert(actions[0].type == SAMPLE_LAUNCHER_ACTION_STOP_Q);
	assert(state.qCurrent == -1);
}

static void test_poly_has_four_independent_slots(void)
{
	sampleLauncherStateInit(&state);
	for (uint8_t tile = 0; tile < 4; tile++)
		assert(sampleLauncherStateTogglePoly(&state, tile));
	assert(!sampleLauncherStateTogglePoly(&state, 4));
	assert(commit() == 4);
	for (uint8_t tile = 0; tile < 4; tile++)
		assert(sampleLauncherStateGetPolySlot(&state, tile) == tile);
}

static void test_poly_stop_frees_slot_at_boundary(void)
{
	sampleLauncherStateInit(&state);
	for (uint8_t tile = 0; tile < 4; tile++)
		assert(sampleLauncherStateTogglePoly(&state, tile));
	commit();
	assert(sampleLauncherStateTogglePoly(&state, 1));
	assert(sampleLauncherStateTogglePoly(&state, 8));
	assert(commit() == 2);
	assert(actions[0].type == SAMPLE_LAUNCHER_ACTION_STOP_POLY);
	assert(actions[1].type == SAMPLE_LAUNCHER_ACTION_START_POLY);
	assert(sampleLauncherStateGetPolySlot(&state, 8) == 1);
}

static void test_q_and_poly_never_share_state(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 5));
	assert(sampleLauncherStateTogglePoly(&state, 5));
	assert(commit() == 2);
	assert(state.qCurrent == 5);
	assert(sampleLauncherStateGetPolySlot(&state, 5) == 0);
	assert(sampleLauncherStateRequestQ(&state, 9));
	commit();
	assert(state.qCurrent == 9);
	assert(sampleLauncherStateGetPolySlot(&state, 5) == 0);
}

static void test_banks_keep_distinct_tile_identities(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 0));
	commit();
	assert(state.qCurrent == 0);

	assert(sampleLauncherStateRequestQ(&state, 32));
	commit();
	assert(state.qCurrent == 32);
	assert(sampleLauncherStateTogglePoly(&state, 255));
	commit();
	assert(sampleLauncherStateGetPolySlot(&state, 255) == 0);
	assert(sampleLauncherStateGetPolySlot(&state, 31) == -1);
}

static void test_hard_stop_cancels_waiting_q_only(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 2));
	commit();
	assert(sampleLauncherStateRequestQ(&state, 7));
	assert(sampleLauncherStateRequestQ(&state, 9));
	assert(sampleLauncherStateHardStop(&state, 7, actions) == 0);
	assert(state.qCurrent == 2);
	assert(state.qQueueCount == 1);
	assert(state.qQueue[0] == 9);
}

static void test_hard_stop_kills_q_and_poly_immediately(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateRequestQ(&state, 5));
	assert(sampleLauncherStateTogglePoly(&state, 5));
	commit();
	assert(sampleLauncherStateRequestQ(&state, 8));

	assert(sampleLauncherStateHardStop(&state, 5, actions) == 2);
	assert(actions[0].type == SAMPLE_LAUNCHER_ACTION_STOP_Q);
	assert(actions[0].voice == 0);
	assert(actions[1].type == SAMPLE_LAUNCHER_ACTION_STOP_POLY);
	assert(actions[1].voice == 1);
	assert(state.qCurrent == -1);
	assert(state.qQueueCount == 0);
	assert(sampleLauncherStateGetPolySlot(&state, 5) == -1);
}

static void test_hard_stop_cancels_pending_poly_start(void)
{
	sampleLauncherStateInit(&state);
	assert(sampleLauncherStateTogglePoly(&state, 12));
	assert(sampleLauncherStatePolyStartPending(&state, 12));
	assert(sampleLauncherStateHardStop(&state, 12, actions) == 0);
	assert(!sampleLauncherStatePolyStartPending(&state, 12));
	assert(commit() == 0);
}

int main(void)
{
	test_q_replaces_only_at_boundary();
	test_q_reclick_gracefully_stops();
	test_poly_has_four_independent_slots();
	test_poly_stop_frees_slot_at_boundary();
	test_q_and_poly_never_share_state();
	test_banks_keep_distinct_tile_identities();
	test_hard_stop_cancels_waiting_q_only();
	test_hard_stop_kills_q_and_poly_immediately();
	test_hard_stop_cancels_pending_poly_start();
	puts("9 native Sample Launcher state tests passed.");
	return 0;
}
