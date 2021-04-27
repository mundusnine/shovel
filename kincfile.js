const fs = require('fs-extra');
const child = require('child_process');
const path = require('path');

let project = new Project('Shovel');

project.kore = false;
project.cmd = true;
project.c11 = true;
project.addIncludeDir('Libraries/stb');
project.addIncludeDir('Libraries/cJSON');
project.addIncludeDir('Libraries/curl/include');
project.addIncludeDir('Libraries/par');

fs.ensureDirSync('Deployment');
child.execSync("cmake ./Libraries/curl");
child.execSync('make -C ./Libraries/curl libcurl');
await fs.copyFile('Libraries/curl/lib/libcurl.so','Deployment/libcurl.so');

project.addLib(path.resolve(process.cwd(),'Deployment/libcurl.so'));
project.addFiles(
    "Libraries/par/par_easycurl.h",
    'Libraries/cJSON/cJSON.h',
    'Libraries/cJSON/cJSON.c',
    'Libraries/curl/include/curl/curl.h');
project.addFile('Sources/**');

// project.addCFlag('-lcurl');
project.setDebugDir('Deployment');

resolve(project);
