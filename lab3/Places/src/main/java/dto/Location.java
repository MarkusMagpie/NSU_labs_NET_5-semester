package dto;

public class Location {
    private String name;
    private double lattitude;
    private double longitude;

    public Location(String name, double lattitude, double longitude) {
        this.name = name;
        this.lattitude = lattitude;
        this.longitude = longitude;
    }

    public String getName() {
        return name;
    }
    public void setName(String name) {
        this.name = name;
    }

    public double getLattitude() {
        return lattitude;
    }
    public void setLattitude(double lattitude) {
        this.lattitude = lattitude;
    }

    public double getLongitude() {
        return longitude;
    }
    public void setLongitude(double longitude) {
        this.longitude = longitude;
    }
}
