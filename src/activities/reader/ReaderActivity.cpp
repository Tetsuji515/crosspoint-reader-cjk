#include "ReaderActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>

#include "CrossPointSettings.h"
#include "Epub.h"
#include "EpubReaderActivity.h"
#include "Txt.h"
#include "TxtReaderActivity.h"
#include "Xtc.h"
#include "XtcReaderActivity.h"
#include "activities/util/BmpViewerActivity.h"
#include "activities/util/FontPreviewActivity.h"
#include "util/FileTypeUtils.h"

std::unique_ptr<Epub> ReaderActivity::loadEpub(const std::string& path) {
  if (!Storage.exists(path.c_str())) {
    LOG_ERR("READER", "File does not exist: %s", path.c_str());
    return nullptr;
  }

  auto epub = std::unique_ptr<Epub>(new Epub(path, "/.crosspoint"));
  if (epub->load(true, SETTINGS.embeddedStyle == 0)) {
    return epub;
  }

  LOG_ERR("READER", "Failed to load epub");
  return nullptr;
}

std::unique_ptr<Xtc> ReaderActivity::loadXtc(const std::string& path) {
  if (!Storage.exists(path.c_str())) {
    LOG_ERR("READER", "File does not exist: %s", path.c_str());
    return nullptr;
  }

  auto xtc = std::unique_ptr<Xtc>(new Xtc(path, "/.crosspoint"));
  if (xtc->load()) {
    return xtc;
  }

  LOG_ERR("READER", "Failed to load XTC");
  return nullptr;
}

std::unique_ptr<Txt> ReaderActivity::loadTxt(const std::string& path) {
  if (!Storage.exists(path.c_str())) {
    LOG_ERR("READER", "File does not exist: %s", path.c_str());
    return nullptr;
  }

  auto txt = std::unique_ptr<Txt>(new Txt(path, "/.crosspoint"));
  if (txt->load()) {
    return txt;
  }

  LOG_ERR("READER", "Failed to load TXT");
  return nullptr;
}

void ReaderActivity::goToLibrary(const std::string& fromBookPath) {
  // If coming from a book, start in that book's folder; otherwise start from root
  auto initialPath = fromBookPath.empty() ? "/" : FsHelpers::extractFolderPath(fromBookPath);
  activityManager.goToFileBrowser(std::move(initialPath));
}

void ReaderActivity::onGoToEpubReader(std::unique_ptr<Epub> epub) {
  const auto epubPath = epub->getPath();
  currentBookPath = epubPath;
  activityManager.replaceActivity(std::make_unique<EpubReaderActivity>(renderer, mappedInput, std::move(epub)));
}

void ReaderActivity::onGoToImageViewer(const std::string& path) {
  activityManager.replaceActivity(std::make_unique<BmpViewerActivity>(renderer, mappedInput, path));
}

void ReaderActivity::onGoToFontPreview(const std::string& path) {
  activityManager.replaceActivity(
      std::make_unique<FontPreviewActivity>(renderer, mappedInput, path, [this, path] { goToLibrary(path); }));
}

void ReaderActivity::onGoToXtcReader(std::unique_ptr<Xtc> xtc) {
  const auto xtcPath = xtc->getPath();
  currentBookPath = xtcPath;
  activityManager.replaceActivity(std::make_unique<XtcReaderActivity>(renderer, mappedInput, std::move(xtc)));
}

void ReaderActivity::onGoToTxtReader(std::unique_ptr<Txt> txt) {
  const auto txtPath = txt->getPath();
  currentBookPath = txtPath;
  activityManager.replaceActivity(std::make_unique<TxtReaderActivity>(renderer, mappedInput, std::move(txt)));
}

void ReaderActivity::onEnter() {
  Activity::onEnter();

  if (initialBookPath.empty()) {
    goToLibrary();  // Start from root when entering via Browse
    return;
  }

  currentBookPath = initialBookPath;
  switch (FileTypeUtils::getOpenRoute(initialBookPath)) {
    case FileTypeUtils::FileOpenRoute::ImageViewer:
      onGoToImageViewer(initialBookPath);
      return;
    case FileTypeUtils::FileOpenRoute::FontPreview:
      onGoToFontPreview(initialBookPath);
      return;
    case FileTypeUtils::FileOpenRoute::XtcReader: {
      auto xtc = loadXtc(initialBookPath);
      if (!xtc) {
        onGoBack();
        return;
      }
      onGoToXtcReader(std::move(xtc));
      return;
    }
    case FileTypeUtils::FileOpenRoute::TxtReader: {
      auto txt = loadTxt(initialBookPath);
      if (!txt) {
        onGoBack();
        return;
      }
      onGoToTxtReader(std::move(txt));
      return;
    }
    case FileTypeUtils::FileOpenRoute::EpubReader: {
      auto epub = loadEpub(initialBookPath);
      if (!epub) {
        onGoBack();
        return;
      }
      onGoToEpubReader(std::move(epub));
      return;
    }
    case FileTypeUtils::FileOpenRoute::Unsupported:
      goToLibrary(initialBookPath);
      return;
  }
}

void ReaderActivity::onGoBack() { finish(); }
