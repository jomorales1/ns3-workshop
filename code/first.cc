#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/qos-txop.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
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
uint32_t distributionType = 0;
uint32_t routingProtocol = 0;

string trafficType;
uint32_t port = 3000;

void unicast(int numOfServers) {
    int numOfClients = nodes - numOfServers;
    Ipv4Address serversAddresses[numOfServers];

    for (int i = 1; i <= numOfServers; i++) {
        Ptr<Node> server = NodeList::GetNode (nodes - i);
        serversAddresses[i - 1] = server->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

        PacketSinkHelper sink (
            trafficType,
            InetSocketAddress (Ipv4Address::GetAny (), port)
        );
        ApplicationContainer apps = sink.Install (server);
        apps.Start (Seconds (1));
        apps.Stop (Seconds (stopTime));
    }

    for (int c = 0; c < numOfClients; c++) {
        Ptr<Node> client = NodeList::GetNode(c);
        for (int s = 0; s < numOfServers; s++) {
            OnOffHelper onoff (
                trafficType,
                Address (InetSocketAddress (serversAddresses[s], port))
            );

            ApplicationContainer apps = onoff.Install (client);
            apps.Start (Seconds (1));
            apps.Stop (Seconds (stopTime));
        }
    }
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

int main(int argc, char *argv[]) {
    CommandLine cmd (__FILE__);
    cmd.AddValue("stopTime", "Stop time", stopTime);
    cmd.AddValue("packetSize", "Packets' size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("trafficTypeCode", "Traffic Type", trafficTypeCode);
    cmd.AddValue("distributionType", "Distribution type", distributionType);
    cmd.AddValue("routingProtocol", "Routing protocol", routingProtocol);
    cmd.Parse (argc, argv);

    Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue (packetSize));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (dataRate));

    if (stopTime < 6) {
        std::cout << "Simulation time must greater than 5 sec" << std::endl;
        exit (1);
    }

    switch(trafficTypeCode) {
        case 0:
            trafficType = "ns3::UdpSocketFactory";
            break;
        case 1:
            trafficType = "ns3::TcpSocketFactory";
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

    NS_LOG_INFO ("Setting routing protocol on nodes");
    Ipv4ListRoutingHelper routingHelper;
    OlsrHelper olsrHelper;
    AodvHelper aodvHelper;
    DsdvHelper dsdvHelper;
    DsrHelper dsrHelper;
    DsrMainHelper dsrMainHelper;

    InternetStackHelper internet;
    Ipv4StaticRoutingHelper staticRouting;
    routingHelper.Add(staticRouting, 0);

    switch (routingProtocol) {
        case 0:
            NS_LOG_INFO ("Setting OLSR routing protocol on nodes");
            routingHelper.Add(olsrHelper, 1);
            internet.SetRoutingHelper (routingHelper);
            break;
        case 1:
            NS_LOG_INFO ("Setting AODV routing protocol on nodes");
            routingHelper.Add(aodvHelper, 1);
            internet.SetRoutingHelper (routingHelper);
            break;
        case 2:
            NS_LOG_INFO ("Setting DSDV routing protocol on nodes");
            routingHelper.Add(dsdvHelper, 1);
            internet.SetRoutingHelper (routingHelper);
            break;
        case 3:
            NS_LOG_INFO ("Setting DSR routing protocol on nodes");
            break;
        default:
            std::cout << "Invalid code for routing protocol" << std::endl;
            exit (1);
    }

    internet.Install (nodesContainer);
    if (routingProtocol == 3) {
        dsrMainHelper.Install(dsrHelper, nodesContainer);
    }

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
            unicast(2);
            break;
        case 1:
            broadcast();
            break;
        default:
            std::cout << "Invalid code for distribution type" << std::endl;
            exit (1);
    }

    NS_LOG_INFO ("Configure Tracing.");
    //
    // Let's set up some ns-2-like ascii traces, using another helper class
    //
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("tracing.tr");
    wifiPhy.EnableAsciiAll (stream);
    internet.EnableAsciiIpv4All (stream);

    wifiPhy.EnablePcap ("traffic-output", netDevices);
    // wifiPhy.EnablePcap ("traffic-output", appSink->GetId (), 0);


    AnimationInterface anim ("first.xml");

    NS_LOG_INFO ("Run Simulation.");
    Simulator::Stop (Seconds (stopTime));
    Simulator::Run ();
    Simulator::Destroy ();
}