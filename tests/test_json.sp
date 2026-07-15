var original = {"user": {"name": "Sapphire", "score": 100}, "active": true, "tags": ["fast", "simple"]};
var rawStr = JSON.stringify(original);

var data = JSON.parse(rawStr);
print(data.user.name);
print(data.user.score);
print(data.active);
print(data.tags[0]);

var str = JSON.stringify(data);
print(str == rawStr);
