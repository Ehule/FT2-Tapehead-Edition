#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ft2_sample_matrix_editor.h"

char *supportedSmpExtensions[] = { "wav", "aiff", "iff", "END_OF_LIST" };
static const UNICHAR *initialPath;

const UNICHAR *getDiskOpSmpPath(void)
{
	return initialPath;
}

char *utf8ToCp850(char *source, bool removeIllegalChars)
{
	(void)removeIllegalChars;
	const size_t length = strlen(source) + 1;
	char *copy = malloc(length);
	if (copy != NULL)
		memcpy(copy, source, length);
	return copy;
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	initialPath = argv[1];
	assert(sampleMatrixBrowserOpen());

	assert(sampleMatrixBrowserGetCount() == 5);
	assert(sampleMatrixBrowserEntryIsDirectory(0));
	assert(!strcmp(sampleMatrixBrowserGetName(0), "subfolder"));
	assert(!strcmp(sampleMatrixBrowserGetName(1), "kick1.wav"));
	assert(!strcmp(sampleMatrixBrowserGetName(2), "kick2.wav"));
	assert(!strcmp(sampleMatrixBrowserGetName(3), "kick10.wav"));
	assert(!strcmp(sampleMatrixBrowserGetName(4), "snare.aiff"));
	assert(sampleMatrixBrowserGetFileCount() == 4);

	sampleMatrixBrowserSelect(1, false, false);
	sampleMatrixBrowserSelect(3, true, false);
	assert(sampleMatrixBrowserGetSelectionCount() == 2);
	assert(!strcmp(sampleMatrixBrowserGetSelectedName(0), "kick1.wav"));
	assert(!strcmp(sampleMatrixBrowserGetSelectedName(1), "kick10.wav"));

	sampleMatrixBrowserSelect(2, false, true);
	assert(sampleMatrixBrowserGetSelectionCount() == 2);
	assert(sampleMatrixBrowserEntryIsSelected(2));
	assert(sampleMatrixBrowserEntryIsSelected(3));

	assert(sampleMatrixBrowserOpenDirectory(0));
	assert(sampleMatrixBrowserGetCount() == 1);
	assert(!strcmp(sampleMatrixBrowserGetName(0), "tom3.iff"));
	sampleMatrixBrowserClose();
	assert(sampleMatrixBrowserOpen());
	assert(sampleMatrixBrowserGetCount() == 1);
	assert(!strcmp(sampleMatrixBrowserGetName(0), "tom3.iff"));
	assert(sampleMatrixBrowserGoParent());
	assert(sampleMatrixBrowserGetCount() == 5);

	sampleMatrixBrowserClose();
	puts("Sample Matrix browser test passed.");
	return 0;
}
