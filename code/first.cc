#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/qos-txop.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/animation-interface.h"
#include <string>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("First");

uint32_t nodes = 20;
uint32_t stopTime = 10;
string packetSize = "1024";
string dataRate = "100kb/s";
uint32_t trafficTypeCode = 0;
uint32_t mobilityType = 0;
uint32_t distributionType = 1;

string trafficType;
uint32_t port = 3000;

// JD START
void unicast() {
    Ptr<Node> appSource = NodeList::GetNode (0);
    Ptr<Node> appSink = NodeList::GetNode (nodes - 1);
    Ipv4Address remoteAddr = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
    OnOffHelper onoff (trafficType,
                        Address (InetSocketAddress (remoteAddr, port)));

    ApplicationContainer apps = onoff.Install (appSource);
    apps.Start (Seconds (1));
    apps.Stop (Seconds (stopTime));

    PacketSinkHelper sink (trafficType,
                    InetSocketAddress (Ipv4Address::GetAny (), port));
    apps = sink.Install (appSink);
    apps.Start (Seconds (1));
}

void broadcast () {
    Address broadcastAddress (InetSocketAddress (Ipv4Address ("255.255.255.255"), port));
    OnOffHelper onOff (trafficType, broadcastAddress);

    ApplicationContainer app = onOff.Install (NodeList::GetNode(0));
    app.Start(Seconds (1.0));
    app.Stop(Seconds (stopTime));
    
    Address receiveFrom (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sink (trafficType, receiveFrom);
                            
    app = sink.Install (NodeList::GetNode (1));
    for (uint32_t i = 2; i < nodes; i++) {
        app.Add(sink.Install (NodeList::GetNode (i)));
    }
    app.Start (Seconds (1.0));
    app.Stop (Seconds (stopTime));  
}

void multicast() {
    
}
// JD FIN

int main(int argc, char *argv[]) {
    CommandLine cmd (__FILE__);
    cmd.AddValue("stopTime", "Stop time", stopTime);
    cmd.AddValue("packetSize", "Packets' size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("trafficTypeCode", "Traffic Type", trafficTypeCode);
    cmd.AddValue("distributionType", "Dsitribution type", distributionType);
    cmd.Parse (argc, argv);

    Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue (packetSize));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));

    switch(trafficTypeCode) {
        case 0:
            trafficType = "ns3::UdpSocketFactory";
            break;
        case 1:
            trafficType = "ns3::TCPSocketFactory";
            break;
        default:
            std::cout << "Invalid code for traffic type" << std::endl;
            exit (1);
    }

    NodeContainer nodesContainer;
    nodesContainer.Create(nodes);

    WifiHelper wifi;
    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue ("OfdmRate54Mbps"));
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    NetDeviceContainer netDevices = wifi.Install (wifiPhy, mac, nodesContainer);

    NS_LOG_INFO ("Enabling OLSR routing on all backbone nodes");
    OlsrHelper olsr;

    InternetStackHelper internet;
    internet.SetRoutingHelper (olsr); // has effect on the next Install ()
    internet.Install (nodesContainer);

    Ipv4AddressHelper ipAddrs;
    ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
    ipAddrs.Assign (netDevices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (20.0),
                                    "MinY", DoubleValue (20.0),
                                    "DeltaX", DoubleValue (20.0),
                                    "DeltaY", DoubleValue (20.0),
                                    "GridWidth", UintegerValue (5),
                                    "LayoutType", StringValue ("RowFirst"));
    mobility.Install(nodesContainer);
    NS_LOG_INFO ("Create Applications.");

    switch (distributionType) {
        case 0:
            unicast();
            break;
        case 1:
            broadcast();
            break;
        case 2:
            multicast();
            break;
        default:
            std::cout << "Invalid code for distribution type" << std::endl;
            exit (1);
    }

    AnimationInterface anim ("first.xml");

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (Seconds (stopTime));
    Simulator::Run ();
    Simulator::Destroy ();
}