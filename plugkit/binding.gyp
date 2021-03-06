{
  "variables":{
    "plugkit_sources":[
      "src/main.cpp",
      "src/pcap.cpp",
      "src/worker_thread.cpp",
      "src/filter_thread.cpp",
      "src/filter_thread_pool.cpp",
      "src/dissector_thread.cpp",
      "src/dissector_thread_pool.cpp",
      "src/stream_dissector_thread.cpp",
      "src/stream_dissector_thread_pool.cpp",
      "src/plugkit_module.cpp",
      "src/allocator.cpp",
      "src/variant.cpp",
      "src/config_map.cpp",
      "src/frame_view.cpp",
      "src/frame_store.cpp",
      "src/filter.cpp",
      "src/token.cpp",
      "src/task.cpp",
      "src/logger.cpp",
      "src/context.cpp",
      "src/null_logger.cpp",
      "src/stream_logger.cpp",
      "src/uvloop_logger.cpp",
      "src/pcap_platform.cpp",
      "src/pcap_dummy.cpp",
      "src/session_context.cpp",
      "src/string_pool.cpp",
      "src/sandbox.cpp",
      "src/module_loader.cpp",
      "src/wrapper/pcap_w.cpp",
      "src/wrapper/session_w.cpp",
      "src/wrapper/frame_w.cpp",
      "src/wrapper/attr_w.cpp",
      "src/wrapper/layer_w.cpp",
      "src/wrapper/payload_w.cpp",
      "src/wrapper/session_factory_w.cpp",
      "src/wrapper/context_w.cpp",
      "src/wrapper/logger_w.cpp",
      "src/wrapper/iterator_w.cpp",
      "src/session.cpp",
      "src/script_dissector.cpp",
      "src/file_importer_task.cpp",
      "src/file_exporter_task.cpp",
      "src/frame.cpp",
      "src/attr.cpp",
      "src/payload.cpp",
      "src/layer.cpp",
      "src/tag_filter.cpp"
    ],
    "plugkit_test_sources":[
      "test/allocator_test.cpp",
      "src/plugkit_testing.cpp",
      "test/token_test.cpp",
      "test/tag_filter_test.cpp",
      "test/payload_test.cpp"
    ]
  },
  "target_defaults":{
    "include_dirs":[
      "vendor/nan",
      "vendor/Catch/include",
      "include/plugkit",
      "src"
    ],
    "conditions":[
      [
        "OS=='linux'",
        {
          "libraries":[
            "-Wl,-dn,-lpcap,-lcap,-lrt,-dy,-lpthread,-ldl",
            "../binding/target/release/libbinding.a"
          ],
          "cflags_cc":[
            "-fno-strict-aliasing"
          ],
          "defines":[
            "PLUGKIT_OS_LINUX"
          ]
        }
      ],
      [
        "OS=='mac'",
        {
          "libraries":[
            "-L/usr/local/lib",
            "-lpcap",
            "../binding/target/release/libbinding.a"
          ],
          "link_settings":{
            "libraries":[
              "$(SDKROOT)/System/Library/Frameworks/Foundation.framework",
              "$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework"
            ]
          },
          "xcode_settings":{
            "MACOSX_DEPLOYMENT_TARGET":"10.9"
          },
          "defines":[
            "PLUGKIT_OS_MAC"
          ]
        }
      ],
      [
        "OS=='win'",
        {
          "include_dirs":[
            "vendor/winpcap/Include"
          ],
          "libraries": [
            '../binding/target/release/binding.lib',
            'Ws2_32.lib',
            'Userenv.lib'
          ],
          "defines":[
            "PLUGKIT_OS_WIN",
            "PLUGKIT_DLL_EXPORT",
            "NOMINMAX"
          ],
          "VCCLCompilerTool": {
            'DisableSpecificWarnings': ['4577'],
            'AdditionalOptions': ['/MP', '/d2guard4']
          },
          "VCLinkerTool": {
            'AdditionalOptions': ['/guard:cf']
          }
        }
      ]
    ]
  },
  "targets":[
    {
      "target_name":"plugkit",
      "sources":[
        "<@(plugkit_sources)",
        "<@(plugkit_test_sources)"
      ],
      "xcode_settings":{
        "GCC_ENABLE_CPP_EXCEPTIONS":"YES"
      },
      "cflags_cc!":[
        "-fno-exceptions"
      ]
    }
  ]
}
