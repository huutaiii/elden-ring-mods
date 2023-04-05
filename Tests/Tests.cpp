#include "CppUnitTest.h"

#include <glm/glm.hpp>
#include <INIReader.h>
#include <MathUtils.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tests
{
	TEST_CLASS(INI)
	{
	public:
		TEST_METHOD(ParseVecNil)
		{
			glm::vec4 out;
			Assert::IsTrue(INIReader::ParseVec("no", out));
			Assert::IsTrue(out == glm::vec4(0));
		}
		TEST_METHOD(ParseVecFail)
		{
			glm::vec3 dummy;
			Assert::IsFalse(INIReader::ParseVec("1, 2", dummy, false));
		}
		TEST_METHOD(ParseVec2)
		{
			glm::vec2 outv2;
			Assert::IsTrue(INIReader::ParseVec("(1, 1)", outv2));
			Assert::IsTrue(outv2 == glm::vec2(1, 1));
		}

		TEST_METHOD(ParseVec2Short)
		{
			glm::vec2 outv2;
			Assert::IsTrue(INIReader::ParseVec("(1,", outv2));
			Assert::IsTrue(outv2 == glm::vec2(1, 0));
		}

		TEST_METHOD(ParseVec3)
		{
			glm::vec3 outv3;
			Assert::IsTrue(INIReader::ParseVec("(1, 1, 1)", outv3));
			Assert::IsTrue(outv3 == glm::vec3(1, 1, 1));
		}
	};

	TEST_CLASS(Math)
	{
	public:
		TEST_METHOD(SmoothStepRange0)
		{
			// how does it not cause a division by 0?
			Assert::IsTrue(smoothstep(1.0, 1.0, 2.0) == 1.0);
			Assert::IsTrue(smoothstep(1.0, 1.0, 0.5) == 0.0);
		}
	};
}
