package dto;

public class Place {
    private String name;
    private String xid; // id места

    public Place(String name, String xid) {
        this.name = name;
        this.xid = xid;
    }

    public String getName() {
        return name;
    }
    public void setName(String name) {
        this.name = name;
    }

    public String getXid() {
        return xid;
    }
    public void setXid(String xid) {
        this.xid = xid;
    }

    @Override
    public String toString() {
        return name + " (" + xid + ")";
    }
}
