// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#if HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/mic.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_hw_shader_enabled = values.use_hw_shader;
    VideoCore::g_hw_shader_accurate_gs = values.shaders_accurate_gs;
    VideoCore::g_hw_shader_accurate_mul = values.shaders_accurate_mul;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_bg_color_update_requested = true;

    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        Core::DSP().SetSink(values.sink_id, values.audio_device_id);
        Core::DSP().EnableStretching(values.enable_audio_stretching);

        auto hid = Service::HID::GetModule(system);
        if (hid) {
            hid->ReloadInputDevices();
        }

        auto sm = system.ServiceManager();
        auto ir_user = sm.GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user)
            ir_user->ReloadInputDevices();
        auto ir_rst = sm.GetService<Service::IR::IR_RST>("ir:rst");
        if (ir_rst)
            ir_rst->ReloadInputDevices();

        auto cam = Service::CAM::GetModule(system);
        if (cam) {
            cam->ReloadCameraDevices();
        }
    }
    // TODO support mic hotswapping by creating the new impl, and copying any parameters to it.
    switch (Settings::values.mic_input_type) {
    case Settings::MicInputType::None:
        Frontend::Mic::RegisterMic(std::make_shared<Frontend::Mic::NullMic>());
        break;
    case Settings::MicInputType::Real:
#if HAVE_CUBEB
        Frontend::Mic::RegisterMic(std::make_shared<AudioCore::CubebInput>());
#endif
        break;
    case Settings::MicInputType::Static:
        Frontend::Mic::RegisterMic(std::make_shared<Frontend::Mic::StaticMic>());
        break;
    }
}

template <typename T>
void LogSetting(const std::string& name, const T& value) {
    LOG_INFO(Config, "{}: {}", name, value);
}

void LogSettings() {
    LOG_INFO(Config, "Citra Configuration:");
    LogSetting("Core_UseCpuJit", Settings::values.use_cpu_jit);
    LogSetting("Renderer_UseGLES", Settings::values.use_gles);
    LogSetting("Renderer_UseHwRenderer", Settings::values.use_hw_renderer);
    LogSetting("Renderer_UseHwShader", Settings::values.use_hw_shader);
    LogSetting("Renderer_ShadersAccurateGs", Settings::values.shaders_accurate_gs);
    LogSetting("Renderer_ShadersAccurateMul", Settings::values.shaders_accurate_mul);
    LogSetting("Renderer_UseShaderJit", Settings::values.use_shader_jit);
    LogSetting("Renderer_UseResolutionFactor", Settings::values.resolution_factor);
    LogSetting("Renderer_VsyncEnabled", Settings::values.vsync_enabled);
    LogSetting("Renderer_UseFrameLimit", Settings::values.use_frame_limit);
    LogSetting("Renderer_FrameLimit", Settings::values.frame_limit);
    LogSetting("Layout_Toggle3d", Settings::values.toggle_3d);
    LogSetting("Layout_Factor3d", Settings::values.factor_3d);
    LogSetting("Layout_LayoutOption", static_cast<int>(Settings::values.layout_option));
    LogSetting("Layout_SwapScreen", Settings::values.swap_screen);
    LogSetting("Audio_EnableDspLle", Settings::values.enable_dsp_lle);
    LogSetting("Audio_EnableDspLleMultithread", Settings::values.enable_dsp_lle_multithread);
    LogSetting("Audio_OutputEngine", Settings::values.sink_id);
    LogSetting("Audio_EnableAudioStretching", Settings::values.enable_audio_stretching);
    LogSetting("Audio_OutputDevice", Settings::values.audio_device_id);
    using namespace Service::CAM;
    LogSetting("Camera_OuterRightName", Settings::values.camera_name[OuterRightCamera]);
    LogSetting("Camera_OuterRightConfig", Settings::values.camera_config[OuterRightCamera]);
    LogSetting("Camera_OuterRightFlip", Settings::values.camera_flip[OuterRightCamera]);
    LogSetting("Camera_InnerName", Settings::values.camera_name[InnerCamera]);
    LogSetting("Camera_InnerConfig", Settings::values.camera_config[InnerCamera]);
    LogSetting("Camera_InnerFlip", Settings::values.camera_flip[InnerCamera]);
    LogSetting("Camera_OuterLeftName", Settings::values.camera_name[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftConfig", Settings::values.camera_config[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftFlip", Settings::values.camera_flip[OuterLeftCamera]);
    LogSetting("DataStorage_UseVirtualSd", Settings::values.use_virtual_sd);
    LogSetting("System_IsNew3ds", Settings::values.is_new_3ds);
    LogSetting("System_RegionValue", Settings::values.region_value);
    LogSetting("Debugging_UseGdbstub", Settings::values.use_gdbstub);
    LogSetting("Debugging_GdbstubPort", Settings::values.gdbstub_port);
}

void LoadProfile(int index) {
    Settings::values.current_input_profile = Settings::values.input_profiles[index];
    Settings::values.current_input_profile_index = index;
}

void SaveProfile(int index) {
    Settings::values.input_profiles[index] = Settings::values.current_input_profile;
}

void CreateProfile(std::string name) {
    Settings::InputProfile profile = values.current_input_profile;
    profile.name = std::move(name);
    Settings::values.input_profiles.push_back(std::move(profile));
    Settings::values.current_input_profile_index =
        static_cast<int>(Settings::values.input_profiles.size()) - 1;
    Settings::LoadProfile(Settings::values.current_input_profile_index);
}

void DeleteProfile(int index) {
    Settings::values.input_profiles.erase(Settings::values.input_profiles.begin() + index);
    Settings::LoadProfile(0);
}

void RenameCurrentProfile(std::string new_name) {
    Settings::values.input_profiles[Settings::values.current_input_profile_index].name =
        std::move(new_name);
}

} // namespace Settings
