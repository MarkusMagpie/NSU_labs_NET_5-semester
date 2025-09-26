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

### 2.1 `thenApply() vs thenCompose()`

1. [`thenApply()`](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CompletableFuture.html#thenApply-java.util.function.Function-)
```java
public <U> CompletableFuture<U> thenApply(Function<? super T,? extends U> fn)
```
> описание:   
> Returns a new CompletionStage that, when this stage completes normally,
> is executed with this `stage's result` as the argument to the supplied function.
>
> параметры:
> - `fn` - the function to use to compute the value of the returned CompletionStage

2. [`thenCompose()`](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CompletableFuture.html#thenCompose-java.util.function.Function-)
```java
public <U> CompletableFuture<U> thenCompose(Function<? super T,? extends CompletionStage<U>> fn)
```
> описание:   
> Returns a new CompletionStage that, when this stage completes normally,
> is executed with `this stage` as the argument to the supplied function.
>
> параметры:
> - `fn` - the function to use to compute the value of the returned CompletionStage

`thenApply()` используется для преобразования результата `CompletableFuture` 
в другой вид, если преобразование не возвращает новый `CompletableFuture`.  
`thenCompose()` применяется, когда преобразование возвращает другой 
`CompletableFuture`, чтобы избежать вложенных будущих объектов.

Пример:
```java
// 1 поиск локации: CompletableFuture<List<Location>>
CompletableFuture<List<Location>> locsFut = api.searchLocations("Москва");

// 2 преобразование List<Location> в первую локацию Location - 
// это делается синхронно:
CompletableFuture<Location> firstLocFut =
    locsFut.thenApply(locs -> locs.get(0));

// 3 на базе локации полученной в 2 делаю второй асинхронный запрос за погодой:
CompletableFuture<Weather> weatherFut =
    firstLocFut.thenCompose(loc -> api.getWeather(loc));
```
шаг 2 - `thenApply()` - преобразование списка в один элемент;  
шаг 3 - `thenCompose()` - из `Location` нужно получить новое `CompletableFuture<Weather>`.

Теперь разобравшись с разницей этих двух методов, опишу как их используем в коде:

### 2.1 метод `main(String[] args)`
```java
public static void main(String[] args) {
    Scanner scanner = new Scanner(System.in);
    ApiService apiService = new ApiService();

    System.out.print("Please enter a name of place: ");
    String query = scanner.nextLine();

    apiService.searchLocations(query)
            .thenApply(locations -> {
                System.out.println("\nFound locations:");
                for (int i = 0; i < locations.size(); i++) {
                    Location loc = locations.get(i);
                    System.out.printf("%d - %s (%.3f, %.3f)\n", i + 1, loc.getName(), loc.getLattitude(), loc.getLongitude());
                }
                System.out.print("\nChoose one of given location numbers: ");
                int choice = scanner.nextInt();
                return locations.get(choice - 1);
            })
            .thenCompose(selectedLocation -> {
                System.out.printf("\nYou chose: %s (%.3f, %.3f)\n", selectedLocation.getName(), selectedLocation.getLattitude(), selectedLocation.getLongitude());
                return apiService.getWeather(selectedLocation)
                        .thenApply(weather -> {
                            System.out.println("Current weather in " + selectedLocation.getName() + ": " + weather);
                            return selectedLocation; // возврат локациии для будущих операций
                        });
            })
            .thenCompose(selectedLocation -> apiService.getPlaces(selectedLocation)
                    .thenCompose(places -> {
                        List<CompletableFuture<Descriptions>> detailsFutures = places.stream()
                                .map(place -> apiService.getDescription(place))
                                .collect(Collectors.toList());

                        CompletableFuture<Void> allDone = CompletableFuture.allOf(
                                detailsFutures.toArray(new CompletableFuture[0])
                        );

                        return allDone.thenApply(v ->
                                detailsFutures.stream()
                                        .map(CompletableFuture::join)
                                        .collect(Collectors.toList())
                        );
                    })
                    .thenApply(placeDetailsList -> {
                        System.out.println("\nSights near " + selectedLocation.getName() + ":");
                        for (int i = 0; i < placeDetailsList.size(); i++) {
                            Descriptions descriptions = placeDetailsList.get(i);
                            System.out.printf("%d - %s\n", i + 1, descriptions);
                        }
                        return selectedLocation;
                    })
            )
            .exceptionally(ex -> {
                System.out.println("Error: " + ex.getMessage());
                return null;
            })
            .join();

    scanner.close();
}
```

> ? КАК ВЫБИРАЛ `thenCompose` И `thenApply`?
> - `thenApply` используется для отображения списка локаций и выбора
> пользователем одной из них. Выбор локации не возвращает новый `CompletableFuture`,
> а просто возвращает объект `Location`.
> - `thenCompose` используется когда вызываю `getWeather()` или
> `getPlaces()`, потому что эти методы возвращают `CompletableFuture` и хочу
> продолжить цепочку асинхронных операций.

Метод `main` - точка входа в программу, в которой: 
1. ввожу название места
2. получаю список локаций с координатами
3. выбираю одну из локаций
4. получаю погоду в выбранной локации
5. получаю список интересных мест в близости
6. получаю описание интересных мест

Теперь опишу код метода `main`:
```java
Scanner scanner = new Scanner(System.in);
ApiService apiService = new ApiService();

System.out.print("Please enter a name of place: ");
String query = scanner.nextLine();
```
Создается объект `Scanner` для чтения пользовательского ввода из консоли.  
Создаётся объект `ApiService` для асинхронного взаимодействия с API.  
Пользователю предлагается ввести название места, которое сохраняется в 
переменную `query`.

```java
apiService.searchLocations(query)
        .thenApply(locations -> {
            System.out.println("\nFound locations:");
            for (int i = 0; i < locations.size(); i++) {
                Location loc = locations.get(i);
                System.out.printf("%d - %s (%.3f, %.3f)\n", i + 1, loc.getName(), loc.getLattitude(), loc.getLongitude());
            }
            System.out.print("\nChoose one of given location numbers: ");
            int choice = scanner.nextInt();
            return locations.get(choice - 1);
        })
```
Вызывается метод `searchLocations(query)`, который возвращает 
`CompletableFuture<List<Location>>`. Этот метод отправляет асинхронный 
GET-запрос к GraphHopper API для поиска локаций по строке `query` и 
возвращает список объектов `Location`.  
Далее метод `thenApply` принимает результат `CompletableFuture<List<Location>>` 
и выполняет следующие действия: 
1. Выводит список найденных локаций в консоль;  
2. Запрашивает у пользователя, какую локацию выбрать с помощью `nextInt()`;
3. Возвращает выбранную локацию из списка `locations`.

> `searchLocations` выполняется асинхронно, но внутри `thenApply` происходит 
> синхронный ввод пользователя `nextInt()`, что блокирует выполнение.  
> Это нужно в консольном приложении, так как выбор локации необходим для 
> продолжения цепочки. Результат - `CompletableFuture<Location>`, содержащий 
> выбранную локацию.

```java
        .thenCompose(selectedLocation -> {
            System.out.printf("\nYou chose: %s (%.3f, %.3f)\n", selectedLocation.getName(), selectedLocation.getLattitude(), selectedLocation.getLongitude());
            return apiService.getWeather(selectedLocation)
                    .thenApply(weather -> {
                        System.out.println("Current weather in " + selectedLocation.getName() + ": " + weather);
                        return selectedLocation; // возврат локациии для будущих операций
                    });
        })
```
Метод `thenCompose()` принимает выбранную локацию `selectedLocation` из 
предыдущего шага.  
Выводится сообщение, подтверждающее выбор пользователя, с названием и 
координатами локации.
Вызывается метод `getWeather(selectedLocation)`, который возвращает 
`CompletableFuture<Weather>`. Он отправляет асинхронный GET-запрос к 
OpenWeatherMap API, используя координаты `selectedLocation` и возвращает объект 
`Weather` с информацией о температуре и описании погоды в `selectedLocation`.  
Внутри `thenApply` результат `getWeather(selectedLocation)` - `weather` 
используется для вывода информации о погоде в консоль.

> Использование `thenCompose` необходимо так как `getWeather(selectedLocation)` 
> возвращает `CompletableFuture<Weather>`, и хочу испольщовать его результат 
> для дальнейшей обработки.

```java
        .thenCompose(selectedLocation -> apiService.getPlaces(selectedLocation)
            .thenCompose(places -> {
                List<CompletableFuture<Descriptions>> detailsFutures = places.stream()
                        .map(place -> apiService.getDescription(place))
                        .collect(Collectors.toList());
    
                CompletableFuture<Void> allDone = CompletableFuture.allOf(
                        detailsFutures.toArray(new CompletableFuture[0])
                );
    
                return allDone.thenApply(v ->
                        detailsFutures.stream()
                                .map(CompletableFuture::join)
                                .collect(Collectors.toList())
                );
            })
            .thenApply(placeDetailsList -> {
                System.out.println("\nSights near " + selectedLocation.getName() + ":");
                for (int i = 0; i < placeDetailsList.size(); i++) {
                    Descriptions descriptions = placeDetailsList.get(i);
                    System.out.printf("%d - %s\n", i + 1, descriptions);
                }
                return selectedLocation;
            })
        )
```
Здесь две вложенные асинхронные операции, выполняющиеся последовательно: 
1. получение списка мест:  
Метод `thenCompose` принимает `selectedLocation` из предыдущего шага.  
Вызывается `getPlaces(selectedLocation)` который возвращает 
`CompletableFuture<List<Place>>`. Он отправляет асинхронный GET-запрос к 
OpenTripMap API для получения списка интересных мест в радиусе от координат 
`selectedLocation`.  
Результат `CompletableFuture<List<Place>>` передается в следующий `thenCompose` 
для обработки.

2. получение описаний для каждого места в списке:  
Для каждого объекта `Place` в списке `places` создается `CompletableFuture<Descriptions>` 
вызовом `getDescription(place)`. Этот метод отправляет асинхронный GET-запрос к 
OpenTripMap API для получения описания места по его идентификатору `xid`.  
Метод `CompletableFuture.allOf` объединяет все `CompletableFuture<Descriptions>` 
в один `CompletableFuture<Void>` который завершается когда все запросы описаний 
завершены.  
Внутри первого `thenApply` используется `join()` для получения результатов 
каждого `CompletableFuture<Descriptions>`, формируя список `List<Descriptions>`.

3. Второй `thenApply` принимает список `placeDetailsList` - список описаний мест
от первого `thenApply`. И для каждого объекта `Descriptions` в списке 
вывожу его в консоль.

> Логика:  
> Первый `thenCompose()` используется, чтобы "распаковать" 
> `CompletableFuture<List<Place>>` и продолжить работу с полученным списком.  
> Второй `thenCompose` обеспечивает обработку списка мест, создавая параллельные 
> запросы для каждого описания.

```java
        .exceptionally(ex -> {
            System.out.println("Error: " + ex.getMessage());
            return null;
        })
        .join();
```
Метод `join()` вызывается на последнем `CompletableFuture` блокируя выполнение 
программы до завершения всех асинхронных операций.  
> Программа дождется завершения всех асинхронных операций перед завершением.