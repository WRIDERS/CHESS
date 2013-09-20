#include <stdint.h>

template <class A_Type,class B_Type> class map{

	public:struct map_data{
		struct map_data* next;
		struct map_data* prev;
		B_Type data;
		A_Type index;
	};
	
	

	public : map_data* ROOT=NULL;
	
	private : static int default_comp(A_Type i1,A_Type i2){
		return (i1==i2);
	}

		
	public : int(*comparator)(A_Type,A_Type)=default_comp;
	
	
	
	public : bool put(A_Type index , B_Type data){
	
		if(ROOT==NULL){
			ROOT=(struct map_data*)malloc(sizeof(struct map_data));
			ROOT->next=NULL;
			ROOT->index=index;
			ROOT->data=data;
			ROOT->prev=NULL;
		}
		else{
			struct map_data* start=ROOT;			
			while(start->next!=NULL){				
				start=start->next;
			}
			start->next=(struct map_data*)malloc(sizeof(struct map_data));
			start->next->prev=start;
			start=start->next;			
			
			start->next=NULL;
			start->index=index;
			start->data=data;						
		}
		return true;	
	}
	
	public : B_Type access(unsigned index){
		unsigned i=1;
		struct map_data* start=ROOT;
		while(start!=NULL){
			if(i==index) return start->data;
			start=start->next;
			i++;
		}
		return NULL;		
	}
	
	public : unsigned indexnum(A_Type index){
		unsigned i=1;
		struct map_data* start=ROOT;
		while(start!=NULL){
			if(comparator(index,start->index)) return i;
			start=start->next;
			i++;
		}
		return 0;			
	}
	
	public : B_Type get(A_Type index){
		struct map_data* start=ROOT;
		while(start!=NULL){

			if(comparator(start->index,index)!=0)
				return start->data;
				
			start=start->next;
		}
		return NULL;
	}

	public : bool remove(A_Type index){
		struct map_data* start=ROOT;
		
		while(start!=NULL){

			if(comparator(start->index,index)!=0){
				if(start==ROOT){
				
					ROOT=ROOT->next;
					if(start->next!=NULL)					
						ROOT->prev=NULL;
					
					free(start);
				}
				else{
					start->prev->next=start->next;
					if(start->next!=NULL)
						start->next->prev=start->prev;
					free(start);	
				}
				break;
			}
								
			start=start->next;	
		}
	
		return true;
	}
	
		
};
