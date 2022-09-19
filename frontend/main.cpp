

#include "tests/Tests.h"

#include <iostream>
#include <cstring>

#include "cpp20Tutorial.h"
#include "cpp14Tutorial.h"
#include "SocketTutorial.h"

#include "coproto/Common/CLP.h"

#include <future>
#include <algorithm>
#include <atomic>
#include <cassert>

int main(int argc, char** argv)
{

	coproto::CLP cmd(argc, argv);

	if (cmd.isSet("u") == false)
	{
		cpp14Tutorial();
		cpp20Tutorial();
		SocketTutorial();
	}
	else
		coproto::testCollection.runIf(cmd);

	return 0;
}