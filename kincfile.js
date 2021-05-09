const fs = require('fs-extra');
const child = require('child_process');
const path = require('path');

const verbose = process.argv.indexOf("--verbose") != -1;

let project = new Project('Shovel');

project.kore = false;
project.cmd = true;
project.c11 = true;
project.addIncludeDir('Libraries/stb');
project.addIncludeDir('Libraries/cJSON');
project.addIncludeDir('Libraries/curl/include');
project.addIncludeDir('Libraries/openssl/include/crypto');
project.addIncludeDir('Libraries/openssl/include/openssl');
project.addIncludeDir('Libraries/par');

fs.ensureDirSync('Deployment');

const opts = verbose ? {stdio: 'inherit'} : {};
child.execSync("cmake ./Libraries/curl -DBUILD_SHARED_LIBS=OFF", opts);
child.execSync('make -C ./Libraries/curl libcurl', opts);
child.execSync('cd ./Libraries/openssl && ./Configure && make build_libs', opts);

project.addLib(path.resolve(process.cwd(),'Libraries/openssl/libcrypto.a'));
project.addLib(path.resolve(process.cwd(),'Libraries/openssl/libssl.a'));

project.addLib(path.resolve(process.cwd(),'Libraries/curl/lib/libcurl.a'));
project.addFiles(
    "Libraries/par/par_easycurl.h",
    'Libraries/cJSON/cJSON.h',
    'Libraries/cJSON/cJSON.c',
    'Libraries/curl/include/curl/curl.h');
project.addFile('Sources/**');

project.addCFlag('-lcrypto');
project.addCFlag('-lssl');
project.setDebugDir('Deployment');

resolve(project);
