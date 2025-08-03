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
    private final String GRAPHHOPPER_API_KEY;
    // https://home.openweathermap.org/api_keys
    private final String OPENWEATHERMAP_API_KEY;
    private final String OPENTRIPMAP_API_KEY;

    public ApiService() {
        this.GRAPHHOPPER_API_KEY = System.getenv("GRAPHHOPPER_API_KEY");
        this.OPENWEATHERMAP_API_KEY = System.getenv("OPENWEATHERMAP_API_KEY");
        this.OPENTRIPMAP_API_KEY = System.getenv("OPENTRIPMAP_API_KEY");
    }

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

    // пример JSON запроса и ответа: https://openweathermap.org/current
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

    public CompletableFuture<Descriptions> getDescription(Place place) {
        String url = "https://api.opentripmap.com/0.1/ru/places/xid/"
                + place.getXid()
                + "?apikey=" + OPENTRIPMAP_API_KEY;
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
}
