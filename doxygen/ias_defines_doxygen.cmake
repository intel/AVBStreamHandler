#!
#! \brief creates a global doc target to run doxygen
#!
macro(IasCreateDoxygenTarget)
  find_package(Doxygen)
  if(DOXYGEN_FOUND AND NOT IAS_DISABLE_DOC)

    set(DOC_INSTALL_TARGETS)
    get_property(PUBLIC_DOC_TARGETS GLOBAL PROPERTY IAS_PUBLIC_DOC_TARGETS)
    if(PUBLIC_DOC_TARGETS)
      add_custom_target(public-doc
        COMMENT "Generating public documentation with Doxygen"
        DEPENDS ${PUBLIC_DOC_TARGETS}
        VERBATIM)
      add_custom_target(install-public-doc
        ${CMAKE_COMMAND} -D COMPONENT=public-doc -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
        DEPENDS public-doc
        COMMENT "Installs public documentation"
        VERBATIM)
      list(APPEND DOC_INSTALL_TARGETS install-public-doc)
    endif()

    get_property(PRIVATE_DOC_TARGETS GLOBAL PROPERTY IAS_PRIVATE_DOC_TARGETS)
    if(PRIVATE_DOC_TARGETS)
      add_custom_target(private-doc
        COMMENT "Generating private documentation with Doxygen"
        DEPENDS ${PRIVATE_DOC_TARGETS}
        VERBATIM)
      add_custom_target(install-private-doc
        ${CMAKE_COMMAND} -D COMPONENT=private-doc -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
        DEPENDS private-doc
        COMMENT "Installs private documentation"
        VERBATIM)
      list(APPEND DOC_INSTALL_TARGETS install-private-doc)
    endif()

    add_custom_target(doc
      COMMENT "Generating public and private documentation with Doxygen"
      DEPENDS ${PUBLIC_DOC_TARGETS} ${PRIVATE_DOC_TARGETS}
      VERBATIM)
    #be sure "doc" target is built before "install" target
    install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --build ${CMAKE_BINARY_DIR} --target doc)")
    add_custom_target(install-doc
      DEPENDS ${DOC_INSTALL_TARGETS}
      COMMENT "Installs public and private documentation"
      VERBATIM)
#   add_dependencies(install-all-but-tests install-doc)
  elseif()
    MESSAGE( STATUS "No Doxygen found on system, 'doc' target not created.")
  endif()
endmacro()



#
# Create documentation of the component
#
# \param[in] _subsystem the subsystem of the component
# \param[in] _componentName the name of the component
# \param[in] _versionMajor the major number of the version
# \param[in] _versionMinor the minor number of the version
# \param[in] _versionRevision the revision number of the version
# \param[in] ... additional optional argument list of componentNames to be processed
# \param[in] ADDITIONAL_DOXYGEN_PUBLIC_INPUT optional list with additional input folders for public documenation
# \param[in] ADDITIONAL_DOXYGEN_PRIVATE_INPUT optional list with additional input folders for private documenation
# \param[in] HTML_EXTRA_FILES_PUBLIC optional list with additional extra files that are copied to the doxygen output folder for public documenation
# \param[in] HTML_EXTRA_FILES_PRIVATE optional list with additional extra files that are copied to the doxygen output folder for private documenation
#
# You have to different possibilities of using this macro.
# I) Document one single entity: Inside the definition of an entity. E.g.
#
#   IasInitLibrary(  subsystemA libraryA versionMajor versionMinor versionRevision )
#   ...
#      content of the libraryA
#   ...
#   IasCreateComponentDocumentation(subsystemA libraryA versionMajor versionMinor versionRevision)
#   IasBuildLibrary()
#
#   Then the ICD/CLD documentation is created for this special entity
#
# II) Document multiple entities at once: Outside the definition of an entity. E.g.
#
#   IasInitLibrary(  subsystemA libraryA versionMajor versionMinor versionRevision )
#   ...
#      content of the libraryA
#   ...
#   IasBuildLibrary()
#   IasInitLibrary(  subsystemA libraryB versionMajor versionMinor versionRevision )
#   ...
#      content of the libraryB
#   ...
#   IasBuildLibrary()
#   IasInitExecutable(  subsystemA executableC versionMajor versionMinor versionRevision )
#   ...
#      content of the executableC
#   ...
#   IasBuildExecutable()
#
#   IasCreateComponentDocumentation(subsystemA nameOfTheDocumentation versionMajor versionMinor versionRevision
#      libraryA
#      libraryB
#      executableC
#   )
#
#   The 'nameOfTheDocumentation' needs not to match any real entity that is covered by the documentation here,
#   but it is not forbidden to be the same as one of the enties.
#
#
# Inside your ICD document you have to give the full qualified path to link to your IDL-HTML documentation
#
# @htmlinclude "subsystemA/libraryA/idl/publicInterfaceA.html"
# @htmlinclude "subsystemA/libraryB/idl/publicInterfaceC.html"
#
# The same counts for your internal interfaces in the CLD
#
# @htmlinclude "internal/subsystemA/libraryA/idl/internalInterfaceB.html"
# @htmlinclude "internal/subsystemA/libraryB/idl/internalInterfaceD.html"
#
#
macro(IasCreateComponentDocumentation _subsystem _componentName _versionMajor _versionMinor _versionRevision)
  find_package(Doxygen)
  if(DOXYGEN_FOUND AND NOT IAS_DISABLE_DOC)
    CMAKE_PARSE_ARGUMENTS(COMPONENT_DOCUMENTATION
      ""
      ""
      "ADDITIONAL_DOXYGEN_PUBLIC_INPUT;ADDITIONAL_DOXYGEN_PRIVATE_INPUT;HTML_EXTRA_FILES_PRIVATE;HTML_EXTRA_FILES_PUBLIC"
      ${ARGN}
      )

    # depending on the usage of the command we have to ensure proper settings
    if ( COMPONENT_DOCUMENTATION_UNPARSED_ARGUMENTS )
      # outside of an entity scope
      set(entities ${COMPONENT_DOCUMENTATION_UNPARSED_ARGUMENTS})
      set(binaryDirectory ${CMAKE_CURRENT_BINARY_DIR})
      set(entityGeneratedDirectories)
      set(APIcopyCommands)
      set(APIcopyDependencies)
      set(APIcopyCommandPrefix)
      foreach (entity ${entities})
        get_property(binaryDir GLOBAL PROPERTY ${_subsystem}-${entity}_EXPORT_BINARY_DIR)

        list(APPEND APIcopyCommands ${APIcopyCommandPrefix} ${CMAKE_COMMAND} -E copy_directory ${binaryDir}/API ${binaryDirectory}/documentation)
        set(APIcopyCommandPrefix COMMAND)

        if (TARGET ias_copy_api_files-${_subsystem}-${entity})
          list(APPEND APIcopyDependencies ias_copy_api_files-${_subsystem}-${entity})
        endif()

        get_property(generatedDir GLOBAL PROPERTY ${_subsystem}-${entity}_EXPORT_GENERATED_DIR)
        list(APPEND entityGeneratedDirectories ${generatedDir})

      endforeach()
    else()
      # inside of an entity scope
      set(entities ${_componentName})
      set(binaryDirectory ${IAS_CURRENT_BINARY_DIR})
      set(APIcopyCommands ${CMAKE_COMMAND} -E copy_directory ${binaryDirectory}/API ${binaryDirectory}/documentation)
      if (TARGET ias_copy_api_files-${_subsystem}-${_componentName})
        set(APIcopyDependencies ias_copy_api_files-${_subsystem}-${_componentName})
      endif()
      set(entityGeneratedDirectories ${IAS_CURRENT_GENERATED_DIR})
    endif()

    file(MAKE_DIRECTORY ${binaryDirectory}/documentation)
    file(COPY
      ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-templates/disclaimer.md
      ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-templates/header.html
      ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-templates/footer.html
      ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-templates/stylesheet.css
      DESTINATION ${binaryDirectory}/documentation)

    #common doc variables
    set(DOXYGEN_PROJECT_NAME "${_subsystem}-${_componentName}")
    set(DOXYGEN_PROJECT_NUMBER "${_versionMajor}.${_versionMinor}.${_versionRevision}")
    set(DOXYGEN_PUBLIC_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/public/doc/images")
    set(DOXYGEN_PUBLIC_INPUT_LIST ${binaryDirectory}/documentation ${CMAKE_CURRENT_SOURCE_DIR}/public/doc ${CMAKE_CURRENT_SOURCE_DIR}/public/inc/media_transport/avb_streamhandler_api ${COMPONENT_DOCUMENTATION_ADDITIONAL_DOXYGEN_PUBLIC_INPUT})
    set(DOXYGEN_PUBLIC_EXCLUDE_LIST ${binaryDirectory}/documentation/internal)

    #public documentation
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/documentation/public/${_subsystem}/${_componentName}/)
    set(DOXYGEN_PUBLIC_IMAGE_PATH_LIST ${DOXYGEN_PUBLIC_IMAGE_PATH})

    foreach(path ${DOXYGEN_PUBLIC_IMAGE_PATH_LIST})
      if(EXISTS ${path})
        set(DOXYGEN_IMAGE_PATHS "${DOXYGEN_IMAGE_PATHS} ${path}")
      endif()
    endforeach()

    set(DOXYGEN_INPUT)
    foreach(input ${DOXYGEN_PUBLIC_INPUT_LIST})
      if (EXISTS ${input})
        set(DOXYGEN_INPUT "${DOXYGEN_INPUT} ${input}")
      endif()
    endforeach()
    set(DOXYGEN_EXAMPLE_PATH "${DOXYGEN_INPUT}")
    foreach (entityGeneratedDir ${entityGeneratedDirectories})
      set(DOXYGEN_EXAMPLE_PATH "${DOXYGEN_EXAMPLE_PATH} ${entityGeneratedDir}")
      list(APPEND DOXYGEN_PUBLIC_EXCLUDE_LIST ${entityGeneratedDir}/internal)
    endforeach()

    # excluded directories
    set(DOXYGEN_EXCLUDE)
    foreach(exclude ${DOXYGEN_PUBLIC_EXCLUDE_LIST})
      set(DOXYGEN_EXCLUDE "${DOXYGEN_EXCLUDE} ${exclude}")
    endforeach()

    # html extra files
    set(DOXYGEN_PUBLIC_HTML_EXTRA_FILES)
    foreach(extraFile ${COMPONENT_DOCUMENTATION_HTML_EXTRA_FILES_PUBLIC})
      set(DOXYGEN_PUBLIC_HTML_EXTRA_FILES "${DOXYGEN_PUBLIC_HTML_EXTRA_FILES} ${extraFile}")
    endforeach()

    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-config/Doxyfile.public.in ${binaryDirectory}/documentation/Doxyfile.public @ONLY)

    add_custom_target(
      public-doc-${_subsystem}-${_componentName}-copy-api
      ${APIcopyCommands}
      COMMENT "Copy API to documentation input for ${_subsystem} ${_componentName}" VERBATIM
    )

  if (APIcopyDependencies)
      add_dependencies(public-doc-${_subsystem}-${_componentName}-copy-api ${APIcopyDependencies})
  endif()

    add_custom_target(public-doc-${_subsystem}-${_componentName}
      ${DOXYGEN_EXECUTABLE} ${binaryDirectory}/documentation/Doxyfile.public 1> ${binaryDirectory}/doxygen.public.log
      WORKING_DIRECTORY ${binaryDirectory}/documentation
      DEPENDS public-doc-${_subsystem}-${_componentName}-copy-api
      COMMENT "Generating public documentation with Doxygen for ${_subsystem} ${_componentName}" VERBATIM
    )

    install(DIRECTORY ${CMAKE_BINARY_DIR}/documentation/public/${_subsystem}/${_componentName}/html/ DESTINATION ${CMAKE_INSTALL_PREFIX}/${IAS_SDK_INSTALL_PATH}/doc/${_subsystem}/${_componentName} COMPONENT public-doc )
    set_property(GLOBAL APPEND PROPERTY IAS_PUBLIC_DOC_TARGETS public-doc-${_subsystem}-${_componentName})
    set_property(GLOBAL APPEND PROPERTY ${_subsystem}-${_componentName}_INSTALLS_FILES-public-doc /${IAS_SDK_INSTALL_PATH}/doc/${_subsystem}/${_componentName}/*)

    #private documentation
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/documentation/private/${_subsystem}/${_componentName}/)
    set(DOXYGEN_PRIVATE_INPUT_LIST ${binaryDirectory}/documentation ${CMAKE_CURRENT_SOURCE_DIR}/private/doc ${CMAKE_CURRENT_SOURCE_DIR}/public/inc ${CMAKE_CURRENT_SOURCE_DIR}/private/inc ${COMPONENT_DOCUMENTATION_ADDITIONAL_DOXYGEN_PRIVATE_INPUT})
    set(DOXYGEN_PRIVATE_IMAGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/private/doc/images")

    set(DOXYGEN_PRIVATE_IMAGE_PATH_LIST ${DOXYGEN_PUBLIC_IMAGE_PATH_LIST} ${DOXYGEN_PRIVATE_IMAGE_PATH})

    set(DOXYGEN_IMAGE_PATHS)
    foreach(path ${DOXYGEN_PRIVATE_IMAGE_PATH_LIST})
      if(EXISTS ${path})
        set(DOXYGEN_IMAGE_PATHS "${DOXYGEN_IMAGE_PATHS} ${path}")
      endif()
    endforeach()


    set(DOXYGEN_INPUT)
    foreach(input ${DOXYGEN_PRIVATE_INPUT_LIST})
      if (EXISTS ${input})
        set(DOXYGEN_INPUT "${DOXYGEN_INPUT} ${input}")
      endif()
    endforeach()

    set(DOXYGEN_EXAMPLE_PATH "${DOXYGEN_INPUT}")
    foreach (entityGeneratedDir ${entityGeneratedDirectories})
      set(DOXYGEN_EXAMPLE_PATH "${DOXYGEN_EXAMPLE_PATH} ${entityGeneratedDir}")
    endforeach()

    set(DOXYGEN_EXCLUDE)

    # html extra files
    set(DOXYGEN_PRIVATE_HTML_EXTRA_FILES)
    foreach(extraFile ${COMPONENT_DOCUMENTATION_HTML_EXTRA_FILES_PRIVATE})
      set(DOXYGEN_PRIVATE_HTML_EXTRA_FILES "${DOXYGEN_PRIVATE_HTML_EXTRA_FILES} ${extraFile}")
    endforeach()

    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen/doxygen-config/Doxyfile.private.in ${binaryDirectory}/documentation/Doxyfile.private @ONLY)

    add_custom_target(private-doc-${_subsystem}-${_componentName} ${DOXYGEN_EXECUTABLE} ${binaryDirectory}/documentation/Doxyfile.private 1> ${binaryDirectory}/doxygen.private.log
      WORKING_DIRECTORY ${binaryDirectory}/documentation
      COMMENT "Generating private documentation with Doxygen for ${_subsystem} ${_componentName}" VERBATIM
    )
#    foreach(entity ${entities})
#      add_dependencies(private-doc-${_subsystem}-${_componentName} ias_code_generator-${_subsystem}-${entity})
#    if (TARGET ias_copy_api_files-${_subsystem}-${entity})
#      add_dependencies(private-doc-${_subsystem}-${_componentName} ias_copy_api_files-${_subsystem}-${entity})
#    endif()
#    endforeach()

    install(DIRECTORY ${CMAKE_BINARY_DIR}/documentation/private/${_subsystem}/${_componentName}/html/ DESTINATION ${CMAKE_INSTALL_PREFIX}/${IAS_SDK_INSTALL_PATH}/design/${_subsystem}/${_componentName} COMPONENT private-doc )
    set_property(GLOBAL APPEND PROPERTY IAS_PRIVATE_DOC_TARGETS private-doc-${_subsystem}-${_componentName})
    set_property(GLOBAL APPEND PROPERTY ${_subsystem}-${_componentName}_INSTALLS_FILES-private-doc /${IAS_SDK_INSTALL_PATH}/design/${_subsystem}/${_componentName}/*)

    set_property(GLOBAL APPEND PROPERTY ${_subsystem}-${_componentName}_PROVIDES-DOC ias-${_subsystem}-${_componentName})
  elseif()
    MESSAGE( STATUS "No Doxygen found on system, 'public-doc-${_subsystem}-${_componentName}' and 'private-doc-${_subsystem}-${_componentName}' target not created.")
  endif()
endmacro()
