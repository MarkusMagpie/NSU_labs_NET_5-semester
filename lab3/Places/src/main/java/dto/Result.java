package dto;

import java.util.List;

public class Result {
    private Weather weather;
    private List<Descriptions> places;

    public Result() {}

    public Result(Weather weather, List<Descriptions> places) {
        this.weather = weather;
        this.places = places;
    }

    public Weather getWeather() {
        return weather;
    }
    public void setWeather(Weather weather) {
        this.weather = weather;
    }

    public List<Descriptions> getPlaces() {
        return places;
    }
    public void setPlaces(List<Descriptions> places) {
        this.places = places;
    }
}
