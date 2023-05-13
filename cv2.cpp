#include "csmaNodes3/core-module.h"
#include "csmaNodes3/point-to-point-module.h"
#include "csmaNodes3/network-module.h"
#include "csmaNodes3/applicatiocsmaNodes-module.h"
#include "csmaNodes3/mobility-module.h"
#include "csmaNodes3/csma-module.h"
#include "csmaNodes3/internet-module.h"
#include "csmaNodes3/netanim-module.h"
#include "csmaNodes3/yacsmaNodes-wifi-helper.h"
#include "csmaNodes3/ssid.h"
#include "csmaNodes3/config-store-module.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP-R1 (eduroam)
//  *    *    *    *
//  |    |    |    |    10.1.1.0    R2
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace csmaNodes3;

csmaNodes_LOG_COMPONENT_DEFINE ("cvicenie2");

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktCount, Time pktInterval ){
	if (pktCount > 0) {
		std::ostringstream sprava;
		sprava << "Ahoj"<< pktCount << '\0';
		Ptr<Packet> p = Create<Packet> ( (uint8_t*)sprava.str().c_str(), (uint16_t) sprava.str().length() + 1 );
		socket->Send (p);

		Simulator::Schedule (pktInterval, &GenerateTraffic, socket, pktCount-1, pktInterval);
	}else{ socket->Close (); }
}

int main (int argc, char *argv[]){
	uint32_t nCsma = 3;
	uint32_t wifiNodesifi = 3;

	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
	LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);


	NodeContainer p2pNodes;//2 smerovace
	p2pNodes.Create (2);

	p2pHelperHelper p2pHelper;
	p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	p2pHelper.SetChannelAttribute ("Delay", StringValue ("2ms"));

	NetDeviceContainer pDevices;
	pDevices = p2pHelper.IcsmaNodestall (p2pNodes);

	NodeContainer csmaNodes; // servery
	csmaNodes.Add (p2pNodes.Get (1));
	csmaNodes.Create (nCsma);

	CsmaHelper csmaHelper;
	csmaHelper.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
	csmaHelper.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

	NetDeviceContainer csmaDevices;
	csmaDevices = csmaHelper.IcsmaNodestall (csmaNodes);

	NodeContainer wifiNodes;
	wifiNodes.Create (wifiNodesifi);

	MobilityHelper mobility;
	mobility.SetPositionAllocator ("csmaNodes3::RandomBoxPositionAllocator"
				,"X", StringValue("csmaNodes3::UniformRandomVariable[Min=0.0|Max=50.0] ")
				,"Y", StringValue("csmaNodes3::UniformRandomVariable[Min=0.0|Max=50.0] ")
				,"Z", StringValue("csmaNodes3::CocsmaNodestantRandomVariable[CocsmaNodestant=1.0] "));
	mobility.SetMobilityModel ("csmaNodes3::RandomWalk2dMobilityModel",
		 "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
	mobility.IcsmaNodestall (wifiNodes);
	mobility.SetMobilityModel ("csmaNodes3::CocsmaNodestantPositionMobilityModel");
	mobility.IcsmaNodestall (p2pNodes.Get (0));
	mobility.IcsmaNodestall (csmaNodes);

	YacsmaNodesWifiChannelHelper channel = YacsmaNodesWifiChannelHelper::Default ();
	YacsmaNodesWifiPhyHelper phy;
	phy.SetChannel (channel.Create ());

	WifiHelper wifi;
	wifi.SetRemoteStationManager ("csmaNodes3::AarfWifiManager");

	WifiMacHelper mac;
	Ssid ssid = Ssid ("eduroam");
	mac.SetType ("csmaNodes3::StaWifiMac", //mobilne zariadenia
	"Ssid", SsidValue (ssid),
	"ActiveProbing", BooleanValue (false));

	NetDeviceContainer staDevices;
	staDevices = wifi.IcsmaNodestall (phy, mac, wifiNodes);
	mac.SetType ("csmaNodes3::ApWifiMac",// R1--2.NIC
	"Ssid", SsidValue (ssid));
	NetDeviceContainer apDevices;
	apDevices = wifi.IcsmaNodestall (phy, mac, p2pNodes.Get (0));

	// siet vrstvu TCP/IP
	InternetStackHelper stack;
	stack.IcsmaNodestall (csmaNodes);
	stack.IcsmaNodestall (p2pNodes.Get (0));
	stack.IcsmaNodestall (wifiNodes);

	Ipv4AddressHelper address;
	address.SetBase ("10.1.1.0", Ipv4Mask("/30"));//4*IP ADSL linku
	Ipv4InterfaceContainer p2pInterfaces;
	p2pInterfaces = address.Assign (pDevices);

	address.SetBase ("10.1.2.0", "255.255.255.0");
	address.Assign (csmaDevices);

	address.SetBase ("10.1.3.0", "255.255.255.0");
	address.Assign (staDevices);
	address.Assign (apDevices);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	UdpEchoServerHelper echoServer (9);
	Ptr<Node> src = wifiNodes.Get(0); Names::Add ("src", src);
	Ptr<Node> dst = csmaNodes.Get(1); Names::Add ("dst", dst);
	ApplicationContainer serverApps = echoServer.IcsmaNodestall (dst);
	serverApps.Start (Seconds (1.0));
	serverApps.Stop (Seconds (10.0));

	auto dstIp = dst->GetObject<Ipv4>();
	csmaNodes_LOG_UNCOND(dstIp->GetAddress(1,0).GetLocal());
	UdpEchoClientHelper echoClient (dstIp->GetAddress(1,0).GetLocal() , 9);//0-> Kt. sietovka NIC, interface               csmaInterfaces.GetAddress (nCsma)
	echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
	echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
	echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

	ApplicationContainer clientApps = echoClient.IcsmaNodestall (src);
	clientApps.Start (Seconds (2.0));
	clientApps.Stop (Seconds (10.0));

	auto ip2 = csmaNodes.Get(2)->GetObject<Ipv4>();
	csmaNodes_LOG_UNCOND(ip2->GetAddress(1,0).GetLocal());
	PacketSinkHelper sinkHelper ("csmaNodes3::UdpSocketFactory", InetSocketAddress (ip2->GetAddress(1,0).GetLocal(), 80));
	TypeId tid = TypeId::LookupByName ("csmaNodes3::UdpSocketFactory");

	Ptr<Socket> recvSink = Socket::CreateSocket (dst, tid);
	InetSocketAddress local = InetSocketAddress (dstIp->GetAddress(1,0).GetLocal(), 80);
	recvSink->Bind (local);

	Ptr<Socket> source = Socket::CreateSocket (wifiNodes.Get(2), tid);
	InetSocketAddress remote = InetSocketAddress (ip2->GetAddress(1,0).GetLocal(), 80);
	source->Connect (remote);
	ApplicationContainer sinkApp = sinkHelper.IcsmaNodestall (csmaNodes.Get(2));
	sinkApp.Start(Seconds(2.0));
	sinkApp.Stop(Seconds(10.0));
	Simulator::Stop (Seconds (10.0));

	Simulator::Schedule (Seconds(4.0), &GenerateTraffic, source, 10, Seconds(0.3));

	p2pHelper.EnablePcapAll ("third");
	phy.EnablePcap ("third", apDevices.Get (0));
	csma.EnablePcap ("third", csmaDevices.Get (0), true);
	
	AnimationInterface a("a.xml");
	a.EnablePacketMetadata();
	a.UpdateNodeColor(dst, 0, 0, 255);
	a.UpdateNodeDescription(dst, "DST");
	a.UpdateNodeColor(src, 0, 255, 0);
	a.UpdateNodeDescription(src, "SRC");
	auto obr = a.AddResource("/home/student/Documents/share/router.png");
	a.UpdateNodeImage(p2pNodes.Get(0)->GetId(), obr);

	Simulator::Run ();
	Simulator::Destroy ();
	return 0;
}