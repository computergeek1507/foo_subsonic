#include "foo_subsonic.h"
#include "sqliteCacheDb.h"
#include <regex>

SqliteCacheDb* SqliteCacheDb::instance = NULL;


SqliteCacheDb::SqliteCacheDb() {
	pfc::string userDir = core_api::get_profile_path(); // save cache to user profile, if enabled
	userDir += "\\foo_subsonic_cache.db";

	userDir = userDir.replace("file://", "");

	db = new SQLite::Database(userDir.c_str(), SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

	if (db != NULL) {
		createTableStructure();
		getAllAlbumsFromCache();
		getAllPlaylistsFromCache();

	}
}

void SqliteCacheDb::createTableStructure() {
	for (unsigned int i = 0; i < SQL_TABLE_CREATE_SIZE; i++) {
		try {
			db->exec(sql_table_create[i]);
		}
		catch (...) {
			uDebugLog() << "Unable to create table: " << sql_table_create[i];
		}
	}
}

SqliteCacheDb::~SqliteCacheDb() {
	if (db != NULL) {
		db->exec("VACUUM;"); // restructure cache, maybe we have to free some space
		db->~Database();
	}
}

std::list<Album>* SqliteCacheDb::getAllAlbums() {
	return &albumlist;
}

Album* SqliteCacheDb::getAllSearchResults() {
	return &searchResults;
}

void SqliteCacheDb::addToUrlMap(Track* t) {
	if (urlToTrackMap.count(t->get_id().c_str()) == 0) {
		urlToTrackMap[t->get_id().c_str()] = t;
	}
}

void SqliteCacheDb::addSearchResult(Track* t) {
	searchResults.addTrack(t);
	addToUrlMap(t);
}

void SqliteCacheDb::addAlbum(Album a) {
	std::list<Track*>* trackList = a.getTracks();
	std::list<Track*>::iterator trackIterator;

	for (trackIterator = trackList->begin(); trackIterator != trackList->end(); trackIterator++) {
		addToUrlMap(*trackIterator);
	}

	albumlist.push_back(a);
}

void SqliteCacheDb::addPlaylist(Playlist p) {
	std::list<Track*>* trackList = p.getTracks();
	std::list<Track*>::iterator trackIterator;

	for (trackIterator = trackList->begin(); trackIterator != trackList->end(); trackIterator++) {
		addToUrlMap(*trackIterator);
	}

	playlists.push_back(p);
}

std::list<Playlist>* SqliteCacheDb::getAllPlaylists() {
	return &playlists;
}

bool SqliteCacheDb::getTrackDetailsByUrl(const char* url, Track &t) {

	std::string strUrl = url;
	std::string result;

	std::regex re(".*id=([^&]+).*");
	std::smatch match;
	if (std::regex_search(strUrl, match, re) && match.size() > 1) { // found ID
		result = match.str(1);
	}
	else { // no ID, cannot continue
		return FALSE;
	}

	if (urlToTrackMap.count(result) > 0) { // fast way
		std::map<std::string, Track*>::iterator i = urlToTrackMap.find(result);
		t = *i->second;
		return TRUE;
	}
	else { // no luck on the fast lane, take the long and slow road ...

		std::list<Album>::iterator it;
		for (it = albumlist.begin(); it != albumlist.end(); it++) {
			std::list<Track*>* trackList = it->getTracks();
			std::list<Track*>::iterator trackIterator;
			for (trackIterator = trackList->begin(); trackIterator != trackList->end(); trackIterator++) {

				uDebugLog() << "Comparing: T->ID: '" << (*trackIterator)->get_id() << "' --- Given ID: '" << result.c_str() << "'";

				if (strcmp((*trackIterator)->get_id().c_str(), result.c_str()) == 0) {
					t = **trackIterator;
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

void SqliteCacheDb::savePlaylists() {
	std::list<Playlist>::iterator it;

	for (it = playlists.begin(); it != playlists.end(); it++) {
		

		SQLite::Statement query(*db, "INSERT OR REPLACE INTO playlists (id, comment, coverArt, duration, public, name, owner, songCount) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);");

		query.bind(1, it->get_id());
		query.bind(2, it->get_comment());
		query.bind(3, it->get_coverArt());
		query.bind(4, it->get_duration());
		query.bind(5, it->get_isPublic());
		query.bind(6, it->get_name());
		query.bind(7, it->get_owner());
		query.bind(8, it->get_songcount());

		while (query.executeStep()) {
			std::list<Track*>* trackList = it->getTracks();
			std::list<Track*>::iterator trackIterator;
			for (trackIterator = trackList->begin(); trackIterator != trackList->end(); trackIterator++) {
				Track* t = *trackIterator;

				SQLite::Statement query_track(*db, "INSERT OR REPLACE INTO playlist_trackss (playlist_id, track_id) VALUES (?1, ?2);");

				query_track.bind(1, t->get_id());
				query_track.bind(2, it->get_id());

				query_track.exec();
			}
		}
	}
}

void SqliteCacheDb::saveAlbums() {
	std::list<Album>::iterator it;
	
	for (it = albumlist.begin(); it != albumlist.end(); it++) {

		SQLite::Statement query(*db, "INSERT OR REPLACE INTO albums (id, artist, title, genre, year, coverArt, duration, songCount) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);");

		query.bind(1, it->get_id());
		query.bind(2, it->get_artist());
		query.bind(3, it->get_title());
		query.bind(4, it->get_genre());
		query.bind(5, it->get_year());
		query.bind(6, it->get_coverArt());
		query.bind(7, it->get_duration());
		query.bind(8, it->get_songCount());

		
		if (query.exec() >= 1) {
			std::list<Track*>* trackList = it->getTracks();
			std::list<Track*>::iterator trackIterator;

			for (trackIterator = trackList->begin(); trackIterator != trackList->end(); trackIterator++) {
				TiXmlElement* track = new TiXmlElement("Track");
				Track* t = *trackIterator;

				SQLite::Statement query_track(*db, "INSERT OR REPLACE INTO tracks (id, title, duration, bitRate, contentType, coverArt, genre, suffix, track, year, size, albumId) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12);");

				query_track.bind(1, t->get_id());
				query_track.bind(2, t->get_title());
				query_track.bind(3, t->get_duration());
				query_track.bind(4, t->get_bitrate());
				query_track.bind(5, t->get_contentType());
				query_track.bind(6, t->get_coverArt());
				query_track.bind(7, t->get_genre());
				query_track.bind(8, t->get_suffix());
				query_track.bind(9, t->get_tracknumber());
				query_track.bind(10, t->get_year());
				query_track.bind(11, t->get_size());
				query_track.bind(12, it->get_id());

				if (query_track.exec() != 1) {				
					uDebugLog() << "Error while inserting track";
				}
			}
		}
	}
}


void SqliteCacheDb::parseTrackInfo(Track *t, SQLite::Statement *query_track) {
	t->set_id(query_track->getColumn(0));
	t->set_parentId(query_track->getColumn(1));
	t->set_title(query_track->getColumn(2));
	t->set_contentType(query_track->getColumn(5));
	t->set_genre(query_track->getColumn(6));
	t->set_suffix(query_track->getColumn(7));
	t->set_year(query_track->getColumn(9));
	t->set_coverArt(query_track->getColumn(11));


	t->set_duration(query_track->getColumn(3));
	t->set_bitrate(query_track->getColumn(4));
	t->set_tracknumber(query_track->getColumn(8));
	t->set_size(query_track->getColumn(10));
}

void SqliteCacheDb::getAllAlbumsFromCache() {
	
	SQLite::Statement query(*db, "SELECT id, artist, title, genre, year, coverArt, duration, songCount FROM albums");

	while (query.executeStep()) {
		Album a;
		a.set_id(query.getColumn(0));
		a.set_artist(query.getColumn(1));
		a.set_title(query.getColumn(2));
		a.set_genre(query.getColumn(3));
		a.set_year(query.getColumn(4));
		a.set_coverArt(query.getColumn(5));
		a.set_duration(query.getColumn(6));
		a.set_songCount(query.getColumn(7));

		SQLite::Statement query_track(*db, "SELECT id, albumId, title, duration, bitrate, contentType, genre, suffix, track, year, size, coverArt FROM tracks WHERE albumId = ?1");
		query_track.bind(1, a.get_id());

		while (query_track.executeStep()) {
			Track* t = new Track();
			parseTrackInfo(t, &query_track);
			t->set_artist(a.get_artist());
			a.addTrack(t);

		}
		albumlist.push_back(a);
	}

}

void SqliteCacheDb::getAllPlaylistsFromCache() {

	SQLite::Statement query(*db, "SELECT id, comment, duration, coverArt, public, name, owner, songCount FROM playlists");

	while (query.executeStep()) {
		Playlist p;

		int i_isPublic = query.getColumn(4);
		bool isPublic = i_isPublic > 0 ? TRUE : FALSE;

		p.set_id(query.getColumn(0));
		p.set_comment(query.getColumn(1));
		p.set_duration(query.getColumn(2));
		p.set_coverArt(query.getColumn(3));
		p.set_isPublic(isPublic);
		p.set_name(query.getColumn(5));
		p.set_owner(query.getColumn(6));
		p.set_songcount(query.getColumn(7));

		SQLite::Statement query_track(*db, "SELECT track_id FROM playlist_tracks WHERE playlist_id = ?1");
		query_track.bind(1, p.get_id());

		while (query_track.executeStep()) {

			SQLite::Statement query_track(*db, "SELECT id, albumId, title, duration, bitrate, contentType, genre, suffix, track, year, size, coverArt FROM tracks WHERE albumId = ?1");
			query_track.bind(1, p.get_id());

			while (query_track.executeStep()) {
				Track* t = new Track();
				parseTrackInfo(t, &query_track);

				SQLite::Statement query_artist(*db, "SELECT artist FROM albums WHERE id = ?1 LIMIT 1");
				query_artist.bind(1, t->get_parentId());

				if (query_artist.executeStep()) {
					t->set_artist(query_artist.getColumn(1));
				}

				
				p.addTrack(t);
			}			
			playlists.push_back(p);
		}
	}	
}

void SqliteCacheDb::addCoverArtToCache(const char* coverArtId, const void * coverArtData, unsigned int dataLength) {
	if (dataLength > 0 && coverArtId != NULL && strlen(coverArtId) > 0) {
		SQLite::Statement query_coverArt(*db, "INSERT OR REPLACE INTO coverart (id, coverArtData) VALUES (?1, ?2)");

		query_coverArt.bind(1, coverArtId);
		query_coverArt.bind(2, coverArtData, dataLength);

		if (query_coverArt.exec() < 1) {
			uDebugLog() << "Error inserting coverArtData";
		}
	}
}

void SqliteCacheDb::getCoverArtById(const char* coverArtId, char* &coverArtData, unsigned int &dataLength) {
	SQLite::Statement query_coverArt(*db, "SELECT coverArtData FROM coverart WHERE id = ?1 LIMIT 1");
	dataLength = 0;
	query_coverArt.bind(1, coverArtId);

	if (query_coverArt.executeStep()) {

		dataLength = query_coverArt.getColumn(0).getBytes();

		char *tmpBuffer = new char[dataLength];
		memcpy(tmpBuffer, query_coverArt.getColumn(0).getBlob(), dataLength);

		delete[] coverArtData;
		coverArtData = tmpBuffer;
	}
}

void SqliteCacheDb::getCoverArtByTrackId(const char* trackId, std::string &out_coverId, char* &coverArtData, unsigned int &dataLength) {
	SQLite::Statement query_coverId(*db, "SELECT coverArt FROM tracks WHERE id = ?1 LIMIT 1");

	query_coverId.bind(1, trackId);
	if (query_coverId.executeStep()) {
		getCoverArtById(query_coverId.getColumn(0), coverArtData, dataLength);

		out_coverId.append(query_coverId.getColumn(0).getText());
	}
}


void SqliteCacheDb::clearCoverArtCache() {
	db->exec("DROP TABLE coverart;");
	createTableStructure();
	db->exec("VACUUM;");
}