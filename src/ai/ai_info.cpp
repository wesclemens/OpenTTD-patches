/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info.cpp Implementation of AIInfo and AILibrary */

#include "../stdafx.h"

#include "../script/squirrel_class.hpp"
#include "../script/script_scanner.hpp"
#include "ai_info.hpp"
#include "../string.h"
#include "../debug.h"
#include "../rev.h"

static const char *const ai_api_versions[] =
	{ "0.7", "1.0", "1.1", "1.2", "1.3", "1.4", "1.5", "1.6" };

#if defined(WIN32)
#undef GetClassName
#endif /* WIN32 */
template <> const char *GetClassName<AIInfo, ST_AI>() { return "AIInfo"; }

/* static */ void AIInfo::RegisterAPI(Squirrel *engine)
{
	/* Create the AIInfo class, and add the RegisterAI function */
	DefSQClass<AIInfo, ST_AI> SQAIInfo;
	SQAIInfo.PreRegister(engine);
	SQAIInfo.AddConstructor<void (AIInfo::*)(), 1>(engine, "x");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddSetting, "AddSetting");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddLabels, "AddLabels");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_NONE, "CONFIG_NONE");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_RANDOM, "CONFIG_RANDOM");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_BOOLEAN, "CONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_INGAME, "CONFIG_INGAME");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_DEVELOPER, "CONFIG_DEVELOPER");

	/* Pre 1.2 had an AI prefix */
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_NONE, "AICONFIG_NONE");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_RANDOM, "AICONFIG_RANDOM");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_BOOLEAN, "AICONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_INGAME, "AICONFIG_INGAME");

	SQAIInfo.PostRegister(engine);
	engine->AddMethod("RegisterAI", &AIInfo::Constructor, 2, "tx");
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance = NULL;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, 0)) || instance == NULL) return sq_throwerror(vm, "Pass an instance of a child class of AIInfo to RegisterAI");
	AIInfo *info = (AIInfo *)instance;

	ScriptInfo::Constructor constructor (vm);
	SQInteger res = constructor.construct (info);
	if (res != 0) return res;

	ScriptConfigItem config = _start_date_config;
	config.name = xstrdup(config.name);
	config.description = xstrdup(config.description);
	info->config_list.push_front(config);

	if (constructor.engine->MethodExists (constructor.instance, "MinVersionToLoad")) {
		if (!constructor.engine->CallIntegerMethod (constructor.instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an UseAsRandomAI function, call it. */
	if (constructor.engine->MethodExists (constructor.instance, "UseAsRandomAI")) {
		if (!constructor.engine->CallBoolMethod (constructor.instance, "UseAsRandomAI", &info->use_as_random, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->use_as_random = true;
	}
	/* Try to get the API version the AI is written for. */
	if (constructor.engine->MethodExists (constructor.instance, "GetAPIVersion")) {
		if (!constructor.engine->CallStringMethodFromSet (constructor.instance, "GetAPIVersion", ai_api_versions, &info->api_version, MAX_GET_OPS)) {
			DEBUG(script, 1, "Loading info.nut from (%s.%d): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
			return SQ_ERROR;
		}
	} else {
		info->api_version = ai_api_versions[0];
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	constructor.scanner->RegisterScript (info, info->GetName());
	return 0;
}

AIInfo::AIInfo() :
	min_loadable_version(0),
	use_as_random(false),
	api_version(NULL)
{
}

AIInfo::AIInfo (bool ignored)
	: min_loadable_version(0), use_as_random(false),
	  api_version(*lastof(ai_api_versions))
{
	this->main_script.reset (xstrdup ("%_dummy"));
	this->author.reset (xstrdup ("OpenTTD Developers Team"));
	this->name.reset (xstrdup ("DummyAI"));
	this->short_name.reset (xstrdup ("DUMM"));
	this->description.reset (xstrdup ("A Dummy AI that is loaded when your ai/ dir is empty"));
	this->date.reset (xstrdup ("2008-07-26"));
	this->instance_name.reset (xstrdup ("DummyAI"));
	this->version = 1;
}

bool AIInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}


/* static */ void AILibrary::RegisterAPI(Squirrel *engine)
{
	/* Create the AILibrary class, and add the RegisterLibrary function */
	engine->AddClassBegin("AILibrary");
	engine->AddClassEnd();
	engine->AddMethod("RegisterLibrary", &AILibrary::Constructor, 2, "tx");
}

/* static */ SQInteger AILibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new library */
	AILibrary *library = new AILibrary();

	ScriptInfo::Constructor constructor (vm);
	SQInteger res = constructor.construct (library);
	if (res != 0) {
		delete library;
		return res;
	}

	/* Cache the category */
	if (!constructor.check_method ("GetCategory")) {
		delete library;
		return SQ_ERROR;
	}
	char *cat = constructor.engine->CallStringMethodStrdup (constructor.instance, "GetCategory", MAX_GET_OPS);
	if (cat == NULL) {
		delete library;
		return SQ_ERROR;
	}
	library->category.reset (cat);

	/* Register the Library to the base system */
	char name [1024];
	bstrfmt (name, "%s.%s", library->GetCategory(), library->GetInstanceName());
	constructor.scanner->RegisterScript (library, name);

	return 0;
}
