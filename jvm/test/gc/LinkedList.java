
class LinkedList {
	
	public LinkedList next = null;
	public int val = 0;


	public static LinkedList t() {
		LinkedList head = new LinkedList();
		LinkedList temp = head;
		for (int i = 0; i < 1; i++ ) {
			temp.next = new LinkedList();
			temp = temp.next;
		}
		return head;
	}

	public static void main(String[] args) {
		LinkedList h = t();
		System.gc();
	}
}