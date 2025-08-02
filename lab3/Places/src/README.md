# Отчёт по 3 лабораторной работе "Места (Асинхронное сетевое взаимодействие)"

Используя методы асинхронного программирования (например, `CompletableFuture` для 
`Java`) или библиотеки реактивного программирования (`RxJava`, например) 
провзаимодействовать с несколькими публично доступными API и сделать приложение с 
любым интерфейсом, основанное на этих API. При этом API должны использоваться так:

 - Все вызовы должны делаться с помощью HTTP-библиотеки с асинхронных интерфейсом;  
 - Все независимые друг от друга вызовы API должны работать одновременно;  
 - Вызовы API, которые зависят от данных, полученных из предыдущих API, 
должны оформляться в виде асинхронной цепочки вызовов;  
 - Не допускаются блокировки на ожидании промежуточных результатов в цепочке 
вызовов, допустима только блокировка на ожидании конечного результата 
(в случае консольного приложения). Другими словами, вся логика программы 
должна быть оформлена как одна функция, которая возвращает `CompletableFuture` 
(или аналог в вашем ЯП) без блокировок.  

Логика работы:
1. В поле ввода пользователь вводит название чего-то (например "Цветной проезд") 
и нажимает кнопку поиска;
2. Ищутся варианты локаций с помощью метода [1] и показываются пользователю в 
виде списка;
3. Пользователь выбирает одну локацию;
4. С помощью метода [2] ищется погода в локации и показывается пользователю;
5. С помощью метода [3] ищутся интересные места в локации, далее для каждого 
найденного места с помощью метода [4] ищутся описания, всё это показывается пользователю в виде списка.

## 1 класс `ApiService.java`

### 1.1 поля класса
```java
private final HttpClient httpClient = HttpClient.newHttpClient();
private final ObjectMapper mapper = new ObjectMapper();

private final String GRAPHHOPPER_API_KEY = "";
private final String OPENWEATHERMAP_API_KEY = "";
private final String OPENTRIPMAP_API_KEY = "";
```

### 1.2.1 метод `searchLocations(String query)`
```java
public CompletableFuture<List<Location>> searchLocations(String query) {
    String url = "https://graphhopper.com/api/1/geocode?q=" + query + "&key=" + GRAPHHOPPER_API_KEY;
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parseLocations(response.body());
            });
}
```
Метод `searchLocations` отвечает за асинхронный поиск вариантов локаций по 
введённому текстовому запросу `query` с помощью `GraphHopper API`.
```java
String url = "https://graphhopper.com/api/1/geocode?q=" + query + "&key=" + GRAPHHOPPER_API_KEY;
```
`String url` собирает строку запроса подставляя в неё `query` - запрос юзера и 
`GRAPHHOPPER_API_KEY` - ключ API.

```java
HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();
```
Далее создаём объект класса `HttpRequest`, указывая куда послать HTTP-запрос.
Подробнее про класс `HttpRequest` можно узнать [здесь](https://docs.oracle.com/en/java/javase/11/docs/api/java.net.http/java/net/http/HttpRequest.html).
> из документации:  
> An `HttpRequest` instance is built through an `HttpRequest` builder. 
> An HttpRequest builder is obtained from one of the newBuilder methods. 
> A request's URI, headers, and body can be set.

```java
return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
    .thenApply(response -> {
        return parseLocations(response.body());
    });
```
Отправляю созданный выше HTTP-запрос `request` (второй аргумент говорит что 
тело ответа будет в виде строки `String`) асинхронно и получаю `return value`:
`CompletableFuture<HttpResponse<String>>`.  
При этом важно что метод не получает ответ от сервера моментально, сразу даёт 
только значение в которое позже придёт результат.
  
Когда же ответ на запрос дейсвтительно придёт, то `HttpResponse` благодаря 
`HttpResponse.BodyHandlers.ofString()` будет объектом `HttpResponse<String>`. 
Я беру его тело `response.body()` - строку `JSON`, полученную от `GraphHopper` и
вызываю метод описанный ниже - `parseLocations(json)`. 

Таким образом `thenApply` превращает `CompletableFuture<HttpResponse<String>>` 
в `CompletableFuture<List<Location>>`. Подробнее смотри [здесь](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CompletableFuture.html#thenApply-java.util.function.Function-).

> логика вкратце:  
> как только исходный `CompletableFuture<HttpResponse<String>>` успешно завершится 
> и в нём появится значение `response`, автоматически будет вызван 
> `thenApply(полученное значение, ставшее теперь параметром)`.



### 1.2.2 метод `parseLocations(String json)`
```java
public List<Location> parseLocations(String json) {
    try {
        List<Location> locations = new ArrayList<>();
        JsonNode root = mapper.readTree(json);
        JsonNode hits = root.path("hits");

        for (JsonNode hit : hits) {
            String name = hit.path("name").asText();
            JsonNode point = hit.path("point");
            double lat = point.path("lat").asDouble();
            double lon = point.path("lng").asDouble();

            locations.add(new Location(name, lat, lon));
        }

        return locations;
    } catch (Exception e) {
        throw new RuntimeException("JSON parsing error", e);
    }
}
```
Метод `parseLocations` надёжно превращает JSON-текст API в список Location, 
которым легко оперировать в дальнейшем коде.  
```java
List<Location> locations = new ArrayList<>();
```
Создаю пустой список `locations` куда буду класть все найденные локации.

Пример API ответа от `GraphHopper API` в json формате:
```json
{
    hits: [
        {
            point: {
                lat: 38.8950368,
                lng: -77.0365427
            },
            extent: […],
            name: "Washington",
            country: "United States",
            countrycode: "US",
            state: "District of Columbia",
            osm_id: 5396194,
            osm_type: "R",
            osm_key: "place",
            osm_value: "city"
        }
    ],
    locale: "default"
} 
```
Идем дальше разбирать метод.
```java
JsonNode root = mapper.readTree(json);
JsonNode hits = root.path("hits");
```
Использую `Jackson ObjectMapper` для разбора JSON-строки в структуру `JsonNode`. 
Подробнее [здесь](https://www.baeldung.com/jackson-object-mapper-tutorial#bd-3-json-to-jackson-jsonnode).
> Alternatively, a  `JSON` can be parsed into a `JsonNode` object and used to 
> retrieve data from a specific node:  
> 
> String json = "{ \"color\" : \"Black\", \"type\" : \"FIAT\" }";  
> JsonNode jsonNode = objectMapper.readTree(json);  
> String color = jsonNode.get("color").asText();  
> // Output: color -> Black  

Я делаю то же самое для извлечения узла `hits` из корневого узла.

```java
for (JsonNode hit : hits) {
    String name = hit.path("name").asText();
    JsonNode point = hit.path("point");
    double lat = point.path("lat").asDouble();
    double lon = point.path("lng").asDouble();

    locations.add(new Location(name, lat, lon));
}
```
Далее итерирую по каждому элементу узла `hit`: беру поле `name`, перехожу 
во вложенный узел `point` и извлекаю координаты объекта `lat` и `lon`. 
Извлеченные значения добавляю в список `locations`.



### 1.2.3 метод `getWeather(Location location)`
```java
public CompletableFuture<Weather> getWeather(Location location) {
    String url = "https://api.openweathermap.org/data/2.5/weather?lat="
            + location.getLattitude()
            + "&lon=" + location.getLongitude()
            + "&appid=" + OPENWEATHERMAP_API_KEY
            + "&units=metric";
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parseWeather(response.body());
            });
}
```
Метод `getWeather` отвечает за асинхронный поиск погоды в введённой 
локации `location` с помощью `OpenWeatherMap API`.

Пример API вызова смотри [здесь](https://openweathermap.org/current):
> выглядит так:  
> `https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={API key}`

По аналогии с методом `searchLocations`: 
 - формирую URL запроса
 - создаю `HttpRequest` указывая в нём адрес для `GET` запроса
 - асинхронно отправляю запрос с помощью `sendAsync` и сразу полчуаю ответом
`CompletableFuture<HttpResponse<String>>`
 - ответ обрабатывается с помощью `thenApply` в котором для тела полученного 
ответа вызывается метод `parseWeather`, который возвращает объект 
`CompletableFuture<Weather>`.



### 1.2.4 метод `parseWeather(String json)`
```java
public Weather parseWeather(String json) {
    try {
        JsonNode root = mapper.readTree(json);
        double temp = root.path("main").path("temp").asDouble();
        String desc = root.path("weather").get(0).path("description").asText();

        return new Weather(temp, desc);
    } catch (Exception e) {
        throw new RuntimeException("JSON parsing error", e);
    }
}
```
Пример API ответа в json формате (взято из ссылки [выше](https://openweathermap.org/current)): 
```json
                          
{
   "coord": {
      "lon": 7.367,
      "lat": 45.133
   },
   "weather": [
      {
         "id": 501,
         "main": "Rain",
         "description": "moderate rain",
         "icon": "10d"
      }
   ],
   "base": "stations",
   "main": {
      "temp": 284.2,
      "feels_like": 282.93,
      "temp_min": 283.06,
      "temp_max": 286.82,
      "pressure": 1021,
      "humidity": 60,
      "sea_level": 1021,
      "grnd_level": 910
   },
   "visibility": 10000,
   "wind": {
      "speed": 4.09,
      "deg": 121,
      "gust": 3.47
   },
   "rain": {
      "1h": 2.73
   },
   "clouds": {
      "all": 83
   },
   "dt": 1726660758,
   "sys": {
      "type": 1,
      "id": 6736,
      "country": "IT",
      "sunrise": 1726636384,
      "sunset": 1726680975
   },
   "timezone": 7200,
   "id": 3165523,
   "name": "Province of Turin",
   "cod": 200
}                    
                        
```

```java
JsonNode root = mapper.readTree(json);
```
Использую `Jackson ObjectMapper` для разбора JSON-строки в структуру `JsonNode`.

```java
double temp = root.path("main").path("temp").asDouble();
```
Перехожу в узел `main` из корневого, перехожу во вложенный узел `temp`.  

```java
String desc = root.path("weather").get(0).path("description").asText();
```
Далее перехожу в узел `weather`, беру первый элемент и перехожу во вложенный 
узел `description` и извлекаю текст описания погоды.

```java
return new Weather(temp, desc);
```
Ну и полученные значения `temp` и `desc` передаю в конструктор объекта `Weather`.



### 1.2.5 метод `getPlaces(Location location)`
```java
public CompletableFuture<List<Place>> getPlaces(Location location) {
    String url = "https://api.opentripmap.com/0.1/ru/places/radius"
            + "?radius=1000"
            + "&lat=" + location.getLattitude()
            + "&lon=" + location.getLongitude()
            + "&limit=10"
            + "&apikey=" + OPENTRIPMAP_API_KEY;
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parsePlaces(response.body());
            });
}
```
Метод `getPlaces` отвечает за асинхронный поиск интересных мест в введённой
локации `location` с помощью `OpenTripMap API`.

Пример API вызова смотри [здесь](https://bigballdiary.tistory.com/369):
> выглядит так:  
> `https://api.opentripmap.com/0.1/{lang}/places/radius?radius={radius}&lon={lon}&lat={lat}&apikey={API_KEY}`  
Required parameters  
lang:[string] Language of the data to be requested. (en or ru available)  
radius:[number] Maximum distance to search from center point (in meters)  
lon:[number] Hardness of the center point to be searched  
lat:[number] Latitude of the center point to be searched  
apikey:[string] API key issued by OpenTripMap  

По аналогии с методом `searchLocations`:
- формирую URL запроса
- создаю `HttpRequest` указывая в нём адрес для `GET` запроса
- асинхронно отправляю запрос с помощью `sendAsync` и сразу полчуаю ответом
  `CompletableFuture<HttpResponse<String>>`
- ответ обрабатывается с помощью `thenApply` в котором для тела полученного
  ответа вызывается метод `parseWeather`, который возвращает объект
  `CompletableFuture<List<Place>>`.



### 1.2.6 метод `parsePlaces(String json)`
```java
public List<Place> parsePlaces(String json) {
    try {
        List<Place> list = new ArrayList<>();
        JsonNode root = mapper.readTree(json);
        JsonNode features = root.path("features");
        for (JsonNode f : features) {
            JsonNode properties = f.path("properties");
            String name = properties.path("name").asText();
            String xid  = properties.path("xid").asText();
            
            list.add(new Place(name, xid));
        }

        return list;
    } catch (Exception e) {
        throw new RuntimeException("Failed to parse places", e);
    }
}
```

Пример API ответа в json формате (взято из ссылки [выше](https://bigballdiary.tistory.com/369)):
```json
{
    "type": "FeatureCollection",
    "features": [
        {
            "type": "Feature",
            "id": "15370806",
            "geometry": {
                "type": "Point",
                "coordinates": [
                    126.9778214,
                    37.5664063
                ]
            },
            "properties": {
                "xid": "Q623908",
                "name": "Seoul City Hall",
                "dist": 18.88268149,
                "rate": 7,
                "wikidata": "Q623908",
                "kinds": "historic_architecture,architecture,interesting_places,other_buildings_and_structures"
            }
        },
        {
            "type": "Feature",
            "id": "14934103",
            "geometry": {
                "type": "Point",
                "coordinates": [
                    126.9755554,
                    37.5669441
                ]
            },
            "properties": {
                "xid": "Q12583799",
                "name": "Gyeongung Palace Yangjae",
                "dist": 221.52061994,
                "rate": 7,
                "wikidata": "Q12583799",
                "kinds": "palaces,architecture,historic_architecture,interesting_places"
            }
        }
    ]
}
```

```java
List<Place> list = new ArrayList<>();
```
Создаю пустой список `list` куда буду класть все найденные интересные места.
```java
JsonNode root = mapper.readTree(json);
JsonNode features = root.path("features");
```
Извлекаю узел `features` из корневого узла `root`.  
```java
for (JsonNode f : features) {
    JsonNode properties = f.path("properties");
    String name = properties.path("name").asText();
    String xid  = properties.path("xid").asText();
    
    list.add(new Place(name, xid));
}
```
Перехожу во вложенный узел `properties` и извлекаю название объекта `name` и 
его идентификатор `xid`. Эти извлеченные значения добавляю в список `list`.



### 1.2.7 метод `getDescription(Place place)`
```java
public CompletableFuture<Descriptions> getDescription(Place place) {
    String url = String.format("https://api.opentripmap.com/0.1/ru/places/xid/%s?apikey=%s",
            place.getXid(), OPENTRIPMAP_API_KEY);
    HttpRequest request = HttpRequest.newBuilder(URI.create(url)).GET().build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                String description = parseDescription(response.body());
                return new Descriptions(place.getName(), description);
            });
}
```
Метод `getDescription` отвечает за асинхронный поиск описаний интересных мест 
в введённом конкретном интересном месте `place` с помощью `OpenTripMap API`.

Пример API вызова смотри [здесь](https://bigballdiary.tistory.com/369):
> выглядит так:
> `https://api.opentripmap.com/0.1/en/places/xid/{xid}?apikey={API_KEY}`  
> ! обрати внимание что этот API вызов отличается от схожего из 1.2.5. 
> Теперь извлекается информация о конкретном объекте по его `xid`

По аналогии с методом `searchLocations`:
- формирую URL запроса
- создаю `HttpRequest` указывая в нём адрес для `GET` запроса
- асинхронно отправляю запрос с помощью `sendAsync` и сразу полчуаю ответом
  `CompletableFuture<HttpResponse<String>>`
- ответ обрабатывается с помощью `thenApply` в котором тело полученного
  ответа парсится методом `parseDescription`, который возвращает объект
  `String`. Затем берется имя данного параметом интересного места `place` и
полученное из `parseDescription` его описание. Эти два значения идут в 
конструктор класса `Descriptions`. 



### 1.2.8 метод `parseDescription(String json)`
```java
public String parseDescription(String json) {
    try {
        JsonNode root = mapper.readTree(json);
        JsonNode wiki = root.path("wikipedia_extracts");
        if (wiki.has("text") && !wiki.path("text").asText().isBlank()) {
            return wiki.path("text").asText();
        } else {
            return "No description";
        }
    } catch (Exception e) {
        throw new RuntimeException("Failed to parse place description", e);
    }
}
```
Принимаю параметром JSON-ответ от `OpenTripMap` по конкретному объекту и 
возвращаю из него текст описания.

```java
JsonNode root = mapper.readTree(json);
```
Использую `Jackson ObjectMapper` для разбора JSON-строки в структуру `JsonNode`.

```java
JsonNode wiki = root.path("wikipedia_extracts");
```
Перехожу во вложенный узел `wikipedia_extracts`.

```java
if (wiki.has("text") && !wiki.path("text").asText().isBlank()) {
    return wiki.path("text").asText();
} else {
    return "No description";
}
```
Если в `wikipedia_extracts` есть узел `text` и он не пустой, то возвращаю его, 
иначе возвращаю строку `No description`.



## 2 класс `Main.java`