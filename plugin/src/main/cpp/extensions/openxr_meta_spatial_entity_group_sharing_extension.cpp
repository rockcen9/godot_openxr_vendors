/**************************************************************************/
/*  openxr_meta_spatial_entity_group_sharing_extension.cpp                */
/**************************************************************************/
/*                       This file is part of:                            */
/*                              GODOT XR                                  */
/*                      https://godotengine.org                           */
/**************************************************************************/
/* Copyright (c) 2022-present Godot XR contributors (see CONTRIBUTORS.md) */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "extensions/openxr_meta_spatial_entity_group_sharing_extension.h"

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/open_xrapi_extension.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

OpenXRMetaSpatialEntityGroupSharingExtension *OpenXRMetaSpatialEntityGroupSharingExtension::singleton = nullptr;

OpenXRMetaSpatialEntityGroupSharingExtension *OpenXRMetaSpatialEntityGroupSharingExtension::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(OpenXRMetaSpatialEntityGroupSharingExtension());
	}
	return singleton;
}

OpenXRMetaSpatialEntityGroupSharingExtension::OpenXRMetaSpatialEntityGroupSharingExtension() :
		OpenXRExtensionWrapper() {
	ERR_FAIL_COND_MSG(singleton != nullptr, "An OpenXRMetaSpatialEntityGroupSharingExtension singleton already exists.");

	request_extensions[XR_META_SPATIAL_ENTITY_SHARING_EXTENSION_NAME] = &meta_spatial_entity_sharing_ext;
	request_extensions[XR_META_SPATIAL_ENTITY_GROUP_SHARING_EXTENSION_NAME] = &meta_spatial_entity_group_sharing_ext;
	singleton = this;
}

OpenXRMetaSpatialEntityGroupSharingExtension::~OpenXRMetaSpatialEntityGroupSharingExtension() {
	cleanup();
	singleton = nullptr;
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_group_sharing_supported"), &OpenXRMetaSpatialEntityGroupSharingExtension::is_group_sharing_supported);
}

void OpenXRMetaSpatialEntityGroupSharingExtension::cleanup() {
	meta_spatial_entity_sharing_ext = false;
	meta_spatial_entity_group_sharing_ext = false;
}

Dictionary OpenXRMetaSpatialEntityGroupSharingExtension::_get_requested_extensions(uint64_t p_xr_version) {
	Dictionary result;
	for (auto ext : request_extensions) {
		uint64_t value = reinterpret_cast<uint64_t>(ext.value);
		result[ext.key] = (Variant)value;
	}
	return result;
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_instance_created(uint64_t instance) {
	if (meta_spatial_entity_sharing_ext) {
		bool result = initialize_meta_spatial_entity_sharing_extension((XrInstance)instance);
		if (!result) {
			UtilityFunctions::printerr("Failed to initialize meta_spatial_entity_sharing extension");
			meta_spatial_entity_sharing_ext = false;
		}
	}
}

void OpenXRMetaSpatialEntityGroupSharingExtension::_on_instance_destroyed() {
	cleanup();
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::initialize_meta_spatial_entity_sharing_extension(const XrInstance &p_instance) {
	GDEXTENSION_INIT_XR_FUNC_V(xrShareSpacesMETA);

	return true;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::_on_event_polled(const void *event) {
	if (static_cast<const XrEventDataBuffer *>(event)->type == XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META) {
		on_share_spaces_complete((const XrEventDataShareSpacesCompleteMETA *)event);
		return true;
	}

	return false;
}

bool OpenXRMetaSpatialEntityGroupSharingExtension::share_spaces_with_groups(XrSpace *p_spaces, uint32_t p_space_count, XrUuid *p_groups, uint32_t p_group_count, ShareSpacesCompleteCallback p_callback, void *p_userdata) {
	if (!is_group_sharing_supported()) {
		p_callback(XR_ERROR_EXTENSION_NOT_PRESENT, p_userdata);
		return false;
	}

	XrShareSpacesRecipientGroupsMETA recipient = {
		XR_TYPE_SHARE_SPACES_RECIPIENT_GROUPS_META, // type
		nullptr, // next
		p_group_count, // groupCount
		p_groups, // groups
	};

	XrShareSpacesInfoMETA info = {
		XR_TYPE_SHARE_SPACES_INFO_META, // type
		nullptr, // next
		p_space_count, // spaceCount
		p_spaces, // spaces
		(const XrShareSpacesRecipientBaseHeaderMETA *)&recipient, // recipientInfo
	};

	XrAsyncRequestIdFB request_id;
	const XrResult result = xrShareSpacesMETA(SESSION, &info, &request_id);
	if (!XR_SUCCEEDED(result)) {
		WARN_PRINT("xrShareSpacesMETA failed!");
		WARN_PRINT(get_openxr_api()->get_error_string(result));
		p_callback(result, p_userdata);
		return false;
	}

	requests[request_id] = RequestInfo(p_callback, p_userdata);
	return true;
}

void OpenXRMetaSpatialEntityGroupSharingExtension::on_share_spaces_complete(const XrEventDataShareSpacesCompleteMETA *event) {
	if (!requests.has(event->requestId)) {
		WARN_PRINT("Received unexpected XR_TYPE_EVENT_DATA_SHARE_SPACES_COMPLETE_META");
		return;
	}

	RequestInfo *request = requests.getptr(event->requestId);
	request->callback(event->result, request->userdata);
	requests.erase(event->requestId);
}
