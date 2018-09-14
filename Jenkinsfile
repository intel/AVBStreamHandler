/*!
    @page Jenkinsfile
    Pipeline to build project.

    @section Pipeline dependencies:
    The following are required to run the pipeline:
    @section Required Jenkins varables:

    The following are required to be set in the jenkins job:
    @li http_proxy
    @li https_proxy
    @li ftp_proxy
    @li all_proxy
    @li no_proxy
    @li GIT_PROXY_COMMAND

**/
try {
    node {
        stage('Prerequisites') {
            env.AVB_DEPS='/srv/jenkins/opt/libs'
            sh("mkdir -p $AVB_DEPS")
            sh("rm -rf dlt-daemon")
            sh("git clone https://github.com/GENIVI/dlt-daemon.git")
            dir ('dlt-daemon') {
                dir ('build') {
                    sh("cmake .. -DCMAKE_INSTALL_PREFIX=$AVB_DEPS -DCMAKE_INSTALL_LIBDIR=lib -DWITH_DLT_CXX11_EXT=ON")
                    sh("make && make install")
                }
            }
            sh("rm -rf alsa-lib")
            sh("git clone git://git.alsa-project.org/alsa-lib.git")
            dir ('alsa-lib') {
                sh("git checkout v1.1.6")
                sh("autoreconf -i")
                sh("./configure --prefix=$AVB_DEPS --with-pythonlibs=\"-lpthread -lm -ldl -lpython2.7\" --with-pythonincludes=-I/usr/include/python2.7")
                sh("make && make install")
            }
        }
        stage('Checkout') {
            checkout scm
            sh("git submodule update --init --recursive")       
        }

        stage('Build') {
            echo "Building AVBSH"
            sh "rm -rf build && mkdir build"
            dir ('build') {
                sh "env PKG_CONFIG_PATH=$AVB_DEPS/lib/pkgconfig cmake -DIAS_IS_HOST_BUILD=1 ../"
                sh "make setcap_tool"
                sh "install setcap_tool $AVB_DEPS/bin/"
                sh "sudo setcap cap_setfcap=pe $AVB_DEPS/bin/setcap_tool"
                sh "AVB_SETCAP_TOOL=$AVB_DEPS/bin/setcap_tool make"
            }
        }
    }
}
catch (Exception e) {
    echo 'in expection'
    throw e
}
finally {
    echo 'in finally'
}

