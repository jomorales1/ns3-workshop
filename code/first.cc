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

int main(int argc, char *argv[]) {
    uint32_t nodes = 20;
    uint32_t stopTime = 10;
    string packetSize = "1024";
    string dataRate = "100kb/s";

    CommandLine cmd (__FILE__);
    cmd.AddValue("stopTime", "Stop time", stopTime);
    cmd.AddValue("packetSize", "Packets' size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.Parse (argc, argv);

    Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue (packetSize));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));

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
    uint16_t port = 9;   // Discard port (RFC 863)

    // We want the source to be the first node created outside of the backbone
    // Conveniently, the variable "backboneNodes" holds this node index value
    Ptr<Node> appSource = NodeList::GetNode (0);
    // We want the sink to be the last node created in the topology.
    Ptr<Node> appSink = NodeList::GetNode (nodes - 1);
    // Let's fetch the IP address of the last node, which is on Ipv4Interface 1
    Ipv4Address remoteAddr = appSink->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

    OnOffHelper onoff ("ns3::UdpSocketFactory",
                        Address (InetSocketAddress (remoteAddr, port)));

    ApplicationContainer apps = onoff.Install (appSource);
    apps.Start (Seconds (3));
    apps.Stop (Seconds (stopTime - 1));

    // Create a packet sink to receive these packets
    PacketSinkHelper sink ("ns3::UdpSocketFactory",
                            InetSocketAddress (Ipv4Address::GetAny (), port));
    apps = sink.Install (appSink);
    apps.Start (Seconds (3));

    AnimationInterface anim ("first.xml");

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (Seconds (stopTime));
    Simulator::Run ();
    Simulator::Destroy ();
}