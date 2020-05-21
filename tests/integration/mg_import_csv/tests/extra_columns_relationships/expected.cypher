CREATE INDEX ON :__mg_vertex__(__mg_id__);
CREATE (:__mg_vertex__:`Message`:`Comment` {__mg_id__: 0, `id`: "0", `country`: "Croatia", `browser`: "Chrome", `content`: "yes"});
CREATE (:__mg_vertex__:`Message`:`Comment` {__mg_id__: 1, `id`: "1", `country`: "United Kingdom", `browser`: "Chrome", `content`: "thanks"});
CREATE (:__mg_vertex__:`Message`:`Comment` {__mg_id__: 2, `id`: "2", `country`: "Germany", `content`: "LOL"});
CREATE (:__mg_vertex__:`Message`:`Comment` {__mg_id__: 3, `id`: "3", `country`: "France", `browser`: "Firefox", `content`: "I see"});
CREATE (:__mg_vertex__:`Message`:`Comment` {__mg_id__: 4, `id`: "4", `country`: "Italy", `browser`: "Internet Explorer", `content`: "fine"});
MATCH (u:__mg_vertex__), (v:__mg_vertex__) WHERE u.__mg_id__ = 0 AND v.__mg_id__ = 1 CREATE (u)-[:`LIKES`]->(v);
MATCH (u:__mg_vertex__), (v:__mg_vertex__) WHERE u.__mg_id__ = 0 AND v.__mg_id__ = 2 CREATE (u)-[:`VISITED`]->(v);
MATCH (u:__mg_vertex__), (v:__mg_vertex__) WHERE u.__mg_id__ = 1 AND v.__mg_id__ = 2 CREATE (u)-[:`FOLLOWS`]->(v);
DROP INDEX ON :__mg_vertex__(__mg_id__);
MATCH (u) REMOVE u:__mg_vertex__, u.__mg_id__;