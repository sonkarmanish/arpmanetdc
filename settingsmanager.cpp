#include "settingsmanager.h"
#include "arpmanetdc.h"

//Constructor
SettingsManager::SettingsManager()
{
    //Set initialization status
    initialized = false;
}

SettingsManager::SettingsManager(sqlite3 *db, ArpmanetDC *parent)
{
    //Set objects
    pDB = db;
    pParent = parent;

    //===== Set default values =====

    setDefault(NICKNAME, "Testnick", "nick");
    setDefault(PASSWORD, "test123", "password");
    setDefault(HUB_ADDRESS, "arpmanet.ath.cx", "hubAddress");
    
    //Get default IP string
    QString ipString = pParent->getIPGuess().toString();
    
    setDefault(EXTERNAL_IP, ipString, "externalIP");
    setDefault(DOWNLOAD_PATH, pParent->getDefaultDownloadPath(), "downloadPath");
    setDefault(LAST_DOWNLOAD_FOLDER, pParent->getDefaultDownloadPath(), "lastDownloadToFolder");

    //Build default protocol value
    QByteArray protocolHint;
    foreach (char val, PROTOCOL_MAP)
        protocolHint.append(val);

    setDefault(PROTOCOL_HINT, protocolHint.data(), "protocolHint");
    setDefault(LAST_SEEN_IP, ipString, "lastSeenIP");
    setDefault(HUB_PORT, 4012, "hubPort");
    setDefault(EXTERNAL_PORT, 4012, "externalPort");
    setDefault(AUTO_UPDATE_SHARE_INTERVAL, 3600000, "autoUpdateShareInterval");
    setDefault(SHOW_ADVANCED_MENU, false, "showAdvanced");
    setDefault(SHOW_EMOTICONS, true, "showEmoticons");
    setDefault(FOCUS_PM_ON_NOTIFY, true, "focusPMOnNotify");
    setDefault(MAX_SEARCH_RESULTS, 100, "maxSearchResults"); //Max to give a query, not max to display
    setDefault(MAX_SIMULTANEOUS_DOWNLOADS, 3, "maxSimultaneousDownloads"); //Max to download from queue at a time
    setDefault(MAX_SIMULTANEOUS_UPLOADS, 20, "maxSimultaneousUploads"); //Max to upload to user users at a time - basically like slots to avoid locking up a client with uploads
    setDefault(MAX_MAINCHAT_BLOCKS, 1000, "maxMainchatBlocks"); //Max blocks to display in mainchat
    setDefault(MAX_LABEL_HISTORY_ENTRIES, 20, "maxLabelHistoryEntries"); //Tooltip of status and additional info label
    setDefault(CONTAINER_DIRECTORY, "/Containers/", "containerDirectory"); //The directory in which the containers are stored
    setDefault(FTP_UPDATE_HOST, "ftp://fskbhe2.puk.ac.za", "ftpUpdateHost");
    setDefault(FTP_UPDATE_DIRECTORY, "/pub/Windows/Network/DC/ArpmanetDC/", "ftpUpdateDirectory");
    setDefault(CHECK_FOR_NEW_VERSION_INTERVAL_MS, 1800000, "checkForNewVersionIntervalMS"); //Every 30min
    setDefault(SHARED_MEMORY_KEY, "ArpmanetDCv0.1", "sharedMemoryKey");

    //Set initialization status
    initialized = true;
}

//Destructor
SettingsManager::~SettingsManager()
{
    //Not really much to do here...
}

//--------------------==================== PUBLIC SET FUNCTIONS ====================--------------------

void SettingsManager::setSetting(StringTypeSetting setting, const QString &value)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = value;
}

void SettingsManager::setSetting(IntTypeSetting setting, int value)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = value;
}

void SettingsManager::setSetting(Int64TypeSetting setting, qint64 value)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = value;
}

void SettingsManager::setSetting(BoolTypeSetting setting, bool value)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = value;
}

void SettingsManager::setSetting(int setting, QVariant value)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = value;
}

//--------------------==================== PUBLIC GET FUNCTIONS ====================--------------------

//===== Settings =====
const QString SettingsManager::getSetting(StringTypeSetting setting)    
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).value.toString();
}

int SettingsManager::getSetting(IntTypeSetting setting)
{
    if (!initialized)
        return -1;
    return pSettings.value(setting).value.toInt();
}

qint64 SettingsManager::getSetting(Int64TypeSetting setting)
{
    if (!initialized)
        return -1;
    return pSettings.value(setting).value.toLongLong();
}

bool SettingsManager::getSetting(BoolTypeSetting setting)
{
    if (!initialized)
        return false;
    return pSettings.value(setting).value.toBool();
}

QVariant SettingsManager::getSetting(int setting)
{
    if (!initialized)
        return QVariant();
    return pSettings.value(setting).value;
}

//===== Tags =====
const QString SettingsManager::getTag(StringTypeSetting setting)
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).tag;
}

const QString SettingsManager::getTag(IntTypeSetting setting)
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).tag;
}

const QString SettingsManager::getTag(Int64TypeSetting setting)
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).tag;
}

const QString SettingsManager::getTag(BoolTypeSetting setting)
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).tag;
}

const QString SettingsManager::getTag(int setting)
{
    if (!initialized)
        return QString();

    return pSettings.value(setting).tag;
}

//===== TYPE =====
SettingsManager::SettingType SettingsManager::getType(StringTypeSetting setting)
{
    if (!initialized)
        return SettingUninitialized;

    return pSettings.value(setting).type;
}

SettingsManager::SettingType SettingsManager::getType(IntTypeSetting setting)
{
    if (!initialized)
        return SettingUninitialized;

    return pSettings.value(setting).type;
}

SettingsManager::SettingType SettingsManager::getType(Int64TypeSetting setting)
{
    if (!initialized)
        return SettingUninitialized;

    return pSettings.value(setting).type;
}

SettingsManager::SettingType SettingsManager::getType(BoolTypeSetting setting)
{
    if (!initialized)
        return SettingUninitialized;

    return pSettings.value(setting).type;
}

SettingsManager::SettingType SettingsManager::getType(int setting)
{
    if (!initialized)
        return SettingUninitialized;

    return pSettings.value(setting).type;
}

//===== DEFAULTS =====
bool SettingsManager::isDefault(int setting)
{
    return pSettings.value(setting).value == pSettings.value(setting).defaultValue;
}

//--------------------==================== PUBLIC SET TO DEFAULT ====================--------------------

void SettingsManager::setToDefault(StringTypeSetting setting)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = pSettings.value(setting).defaultValue;
}

void SettingsManager::setToDefault(IntTypeSetting setting)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = pSettings.value(setting).defaultValue;
}

void SettingsManager::setToDefault(Int64TypeSetting setting)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = pSettings.value(setting).defaultValue;
}

void SettingsManager::setToDefault(BoolTypeSetting setting)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = pSettings.value(setting).defaultValue;
}

void SettingsManager::setToDefault(int setting)
{
    ENSURE_INITIALIZED()

    pSettings[setting].value = pSettings.value(setting).defaultValue;
}


//--------------------==================== PUBLIC SAVE AND LOAD FUNCTIONS ====================--------------------

//Load settings from database
bool SettingsManager::loadSettings()
{
    if (!initialized)
        return false;

    QList<QString> queries;
        
    //Read settings from database
    queries.append("SELECT [parameter], [value] FROM Settings;");

    sqlite3_stmt *statement;
    QList<QString> queryErrors(queries);

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i).toUtf8());
        if (sqlite3_prepare_v2(pDB, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (result = sqlite3_step(statement) == SQLITE_ROW)
            {
                //Load settings
                if (cols == 2)
                {
                    QString tag = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 0));
                    QString value = QString::fromUtf16((const unsigned short *)sqlite3_column_text16(statement, 1));
                    if (!tag.isEmpty())
                    {
                        int settingInt = getSettingFromTag(tag);
                        if (settingInt != -1)
                            setSetting(settingInt, value);
                    }
                }
            };
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(pDB);
        if (error != "not an error")
            queryErrors[i] = error;
    }

    return !pSettings.isEmpty();
}

//Save settings to database
bool SettingsManager::saveSettings()
{
    if (!initialized)
        return false;

    QList<QString> queries;
    queries.append("DELETE FROM Settings;");
    
    QList<QString> tagList;
    QList<QVariant> valueList;
    QHashIterator<int, SettingStruct> k(pSettings);
    while (k.hasNext())
    {
        k.next();
        tagList.append(k.value().tag);
        valueList.append(k.value().value);
        queries.append("INSERT INTO Settings([parameter], [value]) VALUES (?, ?);");
    }

    queries.append("COMMIT;");
    queries.append("BEGIN;");

    bool result = true;
    QList<QString> queryErrors(queries);
    sqlite3_stmt *statement;

    //Loop through all queries
    for (int i = 0; i < queries.size(); i++)
    {
        //Prepare a query
        QByteArray query;
        query.append(queries.at(i).toUtf8());
        if (sqlite3_prepare_v2(pDB, query.data(), -1, &statement, 0) == SQLITE_OK)
        {
            if (query.contains("INSERT INTO Settings") && !tagList.isEmpty())
            {
                int res = 0;
                QString parameter = tagList.takeFirst();
                QString value = valueList.takeFirst().toString();
                res = res | sqlite3_bind_text16(statement, 1, parameter.utf16(), parameter.size()*2, SQLITE_STATIC);
                res = res | sqlite3_bind_text16(statement, 2, value.utf16(), value.size()*2, SQLITE_STATIC);
                if (res != SQLITE_OK)
                    result = false;
            }

            int cols = sqlite3_column_count(statement);
            int result = 0;
            while (sqlite3_step(statement) == SQLITE_ROW);
            sqlite3_finalize(statement);    
        }

        //Catch all error messages
        QString error = sqlite3_errmsg(pDB);
        if (error != "not an error")
            queryErrors[i] = error;
    }

    return result;
}

//Get the setting from a tag - WARNING! Slow (takes O(n) time - amort. 50% of linear)
int SettingsManager::getSettingFromTag(const QString &tag)
{
    QHashIterator<int, SettingStruct> i(pSettings);
    while (i.hasNext())
    {
        i.next();
        if (i.value().tag == tag) //Test tag against saved tag and return integer value
            return i.key();
    }

    //Not found
    return -1;
}

//--------------------==================== PRIVATE DEFAULT SET FUNCTIONS ====================--------------------

void SettingsManager::setDefault(StringTypeSetting setting, const QString &value, QString settingTag)
{
    SettingStruct s;
    s.value = value;
    s.defaultValue = value;
    s.tag = settingTag;
    s.type = SettingStringType;

    //Save in settings
    pSettings.insert(setting, s);
}

void SettingsManager::setDefault(IntTypeSetting setting, int value, QString settingTag)
{
    SettingStruct s;
    s.value = value;
    s.defaultValue = value;
    s.tag = settingTag;
    s.type = SettingIntType;
    
    //Save in settings
    pSettings.insert(setting, s);
}

void SettingsManager::setDefault(Int64TypeSetting setting, qint64 value, QString settingTag)
{
    SettingStruct s;
    s.value = value;
    s.defaultValue = value;
    s.tag = settingTag;
    s.type = SettingInt64Type;

    //Save in settings
    pSettings.insert(setting, s);
}

void SettingsManager::setDefault(BoolTypeSetting setting, bool value, QString settingTag)
{
    SettingStruct s;
    s.value = value;
    s.defaultValue = value;
    s.tag = settingTag;
    s.type = SettingBoolType;

    //Save in settings
    pSettings.insert(setting, s);
}