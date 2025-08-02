package service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import dto.*;

import java.net.URI;
import java.net.http.*;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class ApiService {
    private final HttpClient httpClient = HttpClient.newHttpClient();
    private final ObjectMapper mapper = new ObjectMapper();

    // https://graphhopper.com/dashboard/api-keys
    private final String GRAPHHOPPER_API_KEY = "e74a7b4c-9b53-4461-a180-69383c2a2648";
    // https://home.openweathermap.org/api_keys
    private final String OPENWEATHERMAP_API_KEY = "9f27f9c42592cd5cb93fd307aea0719e";
    private final String OPENTRIPMAP_API_KEY = "5ae2e3f221c38a28845f05b616d0c30c49e93cb5171b0d9dc3389917";

    // получение списка локаций
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

    /*
    "hits": [
    {
        "name": "Москва",
        "point": {
            "lat": x,
            "lng": y
        },
        "...": ...
    },
    {...}
    ]
     */
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

    // получение погоды по координатам
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

    // пример JSON ответа и почему парсинг именно такой смотри здесь: https://openweathermap.org/current
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

    // получение списка интеренсых мест в локации
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

    public List<Place> parsePlaces(String json) {
        try {
            List<Place> list = new ArrayList<>();
            JsonNode root = mapper.readTree(json);
            JsonNode features = root.path("features");
            for (JsonNode f : features) {
                JsonNode props = f.path("properties");
                String name = props.path("name").asText();
                String xid  = props.path("xid").asText();
                list.add(new Place(name, xid));
            }

            return list;
        } catch (Exception e) {
            throw new RuntimeException("Failed to parse places", e);
        }
    }

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

    public String parseDescription(String json) {
        try {
            JsonNode root = mapper.readTree(json);
            // попытка взять текст из wikipedia_extracts
            JsonNode wiki = root.path("wikipedia_extracts");
            if (wiki.has("text") && !wiki.path("text").asText().isBlank()) {
                return wiki.path("text").asText();
            }
            return "";  // описания нет
        } catch (Exception e) {
            throw new RuntimeException("Failed to parse place description", e);
        }
    }
}
