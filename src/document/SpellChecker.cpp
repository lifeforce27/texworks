#include "document/SpellChecker.h"
#include "TWUtils.h" // for TWUtils::getLibraryPath
#include "TWApp.h" // FIXME: Remove TWApp dependency (PATH_LIST_SEP)

#include <hunspell.h>

namespace Tw {
namespace Document {

QHash<QString, QString> * SpellChecker::dictionaryList = nullptr;
QHash<const QString,SpellChecker::Dictionary*> * SpellChecker::dictionaries = nullptr;
SpellChecker * SpellChecker::_instance = new SpellChecker();

// static
QHash<QString, QString> * SpellChecker::getDictionaryList(const bool forceReload /* = false */)
{
	if (dictionaryList) {
		if (!forceReload)
			return dictionaryList;
		delete dictionaryList;
	}

	dictionaryList = new QHash<QString, QString>();
	foreach (QDir dicDir, TWUtils::getLibraryPath(QString::fromLatin1("dictionaries")).split(QLatin1String(PATH_LIST_SEP))) {
		foreach (QFileInfo dicFileInfo, dicDir.entryInfoList(QStringList(QString::fromLatin1("*.dic")),
					QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase)) {
			QFileInfo affFileInfo(dicFileInfo.dir(), dicFileInfo.completeBaseName() + QLatin1String(".aff"));
			if (affFileInfo.isReadable())
				dictionaryList->insertMulti(dicFileInfo.canonicalFilePath(), dicFileInfo.completeBaseName());
		}
	}

	emit SpellChecker::instance()->dictionaryListChanged();
	return dictionaryList;
}

// static
SpellChecker::Dictionary * SpellChecker::getDictionary(const QString& language)
{
	if (language.isEmpty())
		return nullptr;

	if (!dictionaries)
		dictionaries = new QHash<const QString, Dictionary*>;

	if (dictionaries->contains(language))
		return dictionaries->value(language);

	foreach (QDir dicDir, TWUtils::getLibraryPath(QString::fromLatin1("dictionaries")).split(QLatin1String(PATH_LIST_SEP))) {
		QFileInfo affFile(dicDir, language + QLatin1String(".aff"));
		QFileInfo dicFile(dicDir, language + QLatin1String(".dic"));
		if (affFile.isReadable() && dicFile.isReadable()) {
			Hunhandle * h = Hunspell_create(affFile.canonicalFilePath().toLocal8Bit().data(),
								dicFile.canonicalFilePath().toLocal8Bit().data());
			dictionaries->insert(language, new Dictionary(language, h));
			return dictionaries->value(language);
		}
	}
	return nullptr;
}

// static
void SpellChecker::clearDictionaries()
{
	if (!dictionaries)
		return;

	foreach(Dictionary * d, *dictionaries)
		delete d;

	delete dictionaries;
	dictionaries = nullptr;
}

SpellChecker::Dictionary::Dictionary(const QString & language, Hunhandle * hunhandle)
	: _language(language)
	, _hunhandle(hunhandle)
	, _codec(nullptr)
{
	if (_hunhandle)
		_codec = QTextCodec::codecForName(Hunspell_get_dic_encoding(_hunhandle));
	if (!_codec)
		_codec = QTextCodec::codecForLocale(); // almost certainly wrong, if we couldn't find the actual name!
}

SpellChecker::Dictionary::~Dictionary()
{
	if (_hunhandle)
		Hunspell_destroy(_hunhandle);
}

bool SpellChecker::Dictionary::isWordCorrect(const QString & word) const
{
	return (Hunspell_spell(_hunhandle, _codec->fromUnicode(word).data()) != 0);
}

QList<QString> SpellChecker::Dictionary::suggestionsForWord(const QString & word) const
{
	QList<QString> suggestions;
	char ** suggestionList;

	int numSuggestions = Hunspell_suggest(_hunhandle, &suggestionList, _codec->fromUnicode(word).data());
	suggestions.reserve(numSuggestions);
	for (int iSuggestion = 0; iSuggestion < numSuggestions; ++iSuggestion)
		suggestions.append(_codec->toUnicode(suggestionList[iSuggestion]));

	Hunspell_free_list(_hunhandle, &suggestionList, numSuggestions);

	return suggestions;
}

void SpellChecker::Dictionary::ignoreWord(const QString & word)
{
	// note that this is not persistent after quitting TW
	Hunspell_add(_hunhandle, _codec->fromUnicode(word).data());
}

} // namespace Document
} // namespace Tw
