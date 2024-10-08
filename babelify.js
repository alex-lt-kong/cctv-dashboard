const fs = require("fs");
const browserify = require("browserify");

const srcList = ["cd.js"];

if (
  process.argv.length !== 3 ||
  (process.argv[2] !== "--prod" && process.argv[2] !== "--dev")
) {
  throw new Error("Shoud specify --prod or --dev");
}

// sample is from tinyify: https://github.com/browserify/tinyify
for (let i = 0; i < srcList.length; ++i) {
  if (process.argv[2] === "--prod") {
    browserify(`./src/${srcList[i]}`)
      .transform("babelify", {
        presets: ["@babel/preset-env", "@babel/preset-react"],
      })
      .transform("unassertify", { global: true })
      .transform("@goto-bus-stop/envify", { global: true })
      .transform("uglifyify", { global: true })
      .plugin("browser-pack-flat/plugin")
      .bundle()
      // .pipe(require('minify-stream')({sourceMap: false})) Doesnt work on Raspberry Pi--causes OOM exception
      .pipe(fs.createWriteStream(`./static/js/${srcList[i]}`));
  } else {
    browserify(`./src/${srcList[i]}`)
      .transform("babelify", {
        presets: ["@babel/preset-env", "@babel/preset-react"],
      })
      .bundle()
      .pipe(fs.createWriteStream(`./static/js/${srcList[i]}`));
  }
}
