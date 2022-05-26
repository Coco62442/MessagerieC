package graph;
public abstract class Edge{
    static int counter = 0; // counter for edge id
    int id;
    String color;
    Vertex[] ends; // table of 2 vertices
    double value;

    public Edge(Vertex v1, Vertex v2, double value , String color ){
        this.id = counter++;
        this.color = color;
        this.ends = new Vertex[2];
        this.ends[0] = v1;
        this.ends[1] = v2;
        this.value = value;
    }

    public Vertex[] getEnds(){
        return this.ends;
    }

    public int getId(){
        return this.id;
    }

	public String getColor() {
		return this.color;
	}

	public double getValue() {
		return this.value;
	}
}