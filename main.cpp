#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <memory>

using namespace std;

#define PRINT(X) cout<<X;
enum class OrderDirection {
	buy = 0, sell = 1
};
enum class OrderType {
	IOC, GFD
};

constexpr char IOCSTR[] = "IOC", GFDSTR[] = "GFD", BUYSTR[] = "BUY", SELLSTR[] =
		"SELL", CANCELSTR[] = "CANCEL", MODIFYSTR[] = "MODIFY", PRINTSTR[] =
		"PRINT";

template<class T>
class Singleton {
public:
	static T& get() {
		static T inst;
		return inst;
	}
protected:
	Singleton() {
	}
	;
};

class Exchange;

class Order {
public:
	Order(OrderDirection direction, OrderType type, int price, int qty,
			const string& oid, int seq) :
			price(price), qty(qty), id(seq), type(type), direction(direction), oid(
					oid) {
	}

	int getQty() const {
		return qty;
	}
	void setQty(int q) {
		qty = q;
	}
	int getPrice() const {
		return price;
	}
	int getId() const {
		return id;
	}
	string getOrderId() const {
		return oid;
	}
	OrderDirection getDir() const {
		return direction;
	}
private:
	int price, qty, id;
	OrderType type;
	OrderDirection direction;
	string oid;
};
using POrder=shared_ptr<Order>;

class Entry {
public:
	int getTotal() const {
		return total;
	}
	bool insert(POrder p) {
		if (!orders.insert( { p->getId(), p }).second)
			return false;
		total += p->getQty();
		return true;
	}
	int erase(int& qty, const string& oid, int price);
	int erase(int id);
	Entry();
private:
	int total;
	map<int, POrder> orders;
	Exchange& X;
};
class Exchange: public Singleton<Exchange> {
public:
	bool insert(OrderDirection od, OrderType ot, int price, int qty,
			const string& oid) {
		if (ot == OrderType::GFD && repo.count(oid))
			return false;
		trade(od, price, qty, oid);
		if (qty > 0 && ot == OrderType::GFD) {
			auto p = make_shared<Order>(od, ot, price, qty, oid, ++sequence);
			repo.insert( { p->getOrderId(), p });
			auto& entries = od == OrderDirection::buy ? buys : sells;
			entries[price].insert(p);
		}
		return true;
	}
	void trade(OrderDirection od, int& price, int& qty, const string& oid) {
		if (od == OrderDirection::buy)
			while (qty > 0) {
				auto i = sells.rbegin();
				if (i == sells.rend() || i->first > price)
					break;
				i->second.erase(qty, oid, price);
			}
		else
			while (qty > 0) {
				auto i = buys.begin();
				if (i == buys.end() || i->first < price)
					break;
				i->second.erase(qty, oid, price);
			}
	}
	POrder getOrder(const string& oid) const {
		auto i = repo.find(oid);
		return i == repo.end() ? nullptr : i->second;
	}
	bool erase(const string& oid) {
		auto i = repo.find(oid);
		if (i == repo.end())
			return false;
		auto p = i->second;
		repo.erase(i);
		erase(buys, p) || erase(sells, p);
		return true;
	}

	void print() {
		PRINT("SELL:\n");
		for (auto i : sells) {
			PRINT(i.first<<'\t'<<i.second.getTotal()<<'\n');
		}
		PRINT("BUY:\n");
		for (auto i : buys) {
			PRINT(i.first<<'\t'<<i.second.getTotal()<<'\n');
		}
		PRINT(endl);
	}
private:
	map<int, Entry, greater<int>> buys, sells;
	map<string, POrder> repo;
	static int sequence;
	bool erase(map<int, Entry, greater<int>>& entries, POrder p) {
		auto i = entries.find(p->getPrice());
		if (i != entries.end()) {
			i->second.erase(p->getId());
			if (!i->second.getTotal())
				entries.erase(i);
			return true;
		}
		return false;
	}
};
int Exchange::sequence = 0;

int Entry::erase(int& qty, const string& oid, int price) {
	while (total > 0) {
		auto i = orders.begin();
		auto p = i->second;
		auto q = p->getQty();
		if (q <= qty) {
			qty -= q;
			total -= q;
			PRINT(
					"TRADE\t"<<p->getOrderId()<<'\t'<<p->getPrice()<<'\t'<<q<<'\t' <<oid<<'\t'<<price<<'\t'<<q<<endl);
			orders.erase(i);
			X.erase(p->getOrderId());
		} else {
			total -= qty;
			PRINT(
					"TRADE\t"<<p->getOrderId()<<'\t'<<p->getPrice()<<'\t'<<qty<<'\t' <<oid<<'\t'<<price<<'\t'<<qty<<endl);
			p->setQty(q - qty);
			qty = 0;
			break;
		}
	}
	return total;
}
int Entry::erase(int id) {
	auto i = orders.find(id);
	if (i == orders.end())
		return false;
	auto p = i->second;
	total -= p->getQty();
	orders.erase(i);
	X.erase(p->getOrderId());
	return total;
}
Entry::Entry() :
		total(0), orders(), X(Exchange::get()) {
}

//base class for different operations
class Operator {
public:
	virtual void process(vector<string> instruction)=0;
	virtual ~Operator() {
	}
	Operator() :
			X(Exchange::get()) {
	}
protected:
	Exchange& X;
};

class TradeOp: public Operator, public Singleton<TradeOp> {
public:
	virtual void process(vector<string> instruction) {
		if (instruction.size() != 5)
			return;
		OrderDirection od;
		if (instruction[0] == BUYSTR)
			od = OrderDirection::buy;
		else if (instruction[0] == SELLSTR)
			od = OrderDirection::sell;
		else
			return;
		OrderType ot;
		if (instruction[1] == IOCSTR)
			ot = OrderType::IOC;
		else if (instruction[1] == GFDSTR)
			ot = OrderType::GFD;
		else
			return;
		int price = stoi(instruction[2]), qty = stoi(instruction[3]);
		if (price <= 0 || qty <= 0)
			return;
		X.insert(od, ot, price, qty, instruction[4]);
	}
	static pair<string, Operator*> createBuyOp() {
		return make_pair(BUYSTR, &TradeOp::get());
	}
	static pair<string, Operator*> createSellOp() {
		return make_pair(SELLSTR, &TradeOp::get());
	}
};

class CancelOp: public Operator, public Singleton<CancelOp> {
public:
	static pair<string, Operator*> createOp() {
		return make_pair(CANCELSTR, &CancelOp::get());
	}
	virtual void process(vector<string> instruction) {
		if (instruction.size() == 2)
			X.erase(instruction[1]);
	}
};
class ModifyOp: public Operator, public Singleton<ModifyOp> {
public:
	static pair<string, Operator*> createOp() {
		return make_pair(MODIFYSTR, &ModifyOp::get());
	}
	virtual void process(vector<string> instruction) {
		if (instruction.size() != 5)
			return;
		OrderDirection od;
		if (instruction[2] == BUYSTR)
			od = OrderDirection::buy;
		else if (instruction[2] == SELLSTR)
			od = OrderDirection::sell;
		else
			return;
		int price = stoi(instruction[3]), qty = stoi(instruction[4]);
		if (price <= 0 || qty <= 0)
			return;
		if (X.erase(instruction[1]))
			X.insert(od, OrderType::GFD, price, qty, instruction[1]);
	}
};
class PrintOp: public Operator, public Singleton<PrintOp> {
public:
	static pair<string, Operator*> createOp() {
		return make_pair(PRINTSTR, &PrintOp::get());
	}
	virtual void process(vector<string> instruction) {
		X.print();
	}
};

class Actor: public Singleton<Actor> {
public:
	Actor() :
			operators( { TradeOp::createBuyOp(), TradeOp::createSellOp(),
					CancelOp::createOp(), ModifyOp::createOp(),
					PrintOp::createOp(), }) {
	}
	void act(const string& line) {
		auto instruction = split(line);
		if (instruction.empty())
			return;
		auto op = operators.find(instruction.front());
		if (op == operators.end())
			return;
		op->second->process(instruction);
	}
private:
	map<string, Operator*> operators;
	vector<string> split(const string& str) {
		const string delim = " ";
		const int delim_len = delim.length();
		vector<string> result;
		size_t start = 0, end;
		for (; (end = str.find(delim, start)) != string::npos;
				start = end + delim_len)
			result.push_back(str.substr(start, end - start));
		result.push_back(str.substr(start));
		return result;
	}
};

int main() {
	std::ifstream in("in.txt");
	std::streambuf *cinbuf = std::cin.rdbuf(); //save old buf
	std::cin.rdbuf(in.rdbuf()); //redirect std::cin to in.txt!

	auto a = Actor::get();
	for (string line; getline(cin, line);) {
		a.act(line);
	}
	std::cin.rdbuf(cinbuf);
	return 0;
}
