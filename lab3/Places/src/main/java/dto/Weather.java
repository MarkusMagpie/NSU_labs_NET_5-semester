package dto;

public class Weather {
    private double temperature;
    private String description;

    public Weather(double temperature, String description) {
        this.temperature = temperature;
        this.description = description;
    }

    public double getTemperature() {
        return temperature;
    }
    public void setTemperature(double temperature) {
        this.temperature = temperature;
    }

    public String getDescription() {
        return description;
    }
    public void setDescription(String description) {
        this.description = description;
    }

    @Override
    public String toString() {
        return String.format("\n\ttemperature: %.3f°C; \n\tdescription: %s.", temperature, description);
    }
}
