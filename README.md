# Generic Network Packet Streaming Interface(gNPSI) 

## Objective

Our objective is to design an API for streaming packet samples from switches to our telemetry infrastructure.  The goal is to replace sFlow/NetFlow because:

* infrastructure UDP transport creates significant challenges around losses (for which we need a well-defined SLO around), especially when the network is under stress which might be when we care most about telemetry. 
* the channel is neither encrypted nor authenticated providing an opening for man-in-the-middle attack if the data is used for core traffic engineering.
* relies on VIP for discovery that has significant blast radius, which is of concern when data is used for real-time traffic engineering
* UDP also complicates the deployment and design of the collection system to account for transport of unencrypted packets.

## Background

Switches provide streaming telemetry via gNMI and gNOI.  gNMI is handled by telemetry collection, which focuses on the state of the device.  Meanwhile gNOI focuses on the operation of the device.  Packet sampling does not appear to fit into either category, in that it is neither related to operation of the device (usually via RPC with limited lifespan), nor related to the state of the device (i.e., an end state can be computed based on the streaming updates).
This proposal suggests gNPSI (gRPC Network Packet Sampling Interface) as the future packet sampling API.  Like gNMI and gNOI, gNPSI would be carried over an authenticated and encrypted gRPC channel.  Similar to gNMI, we expect a streaming channel for samples to be readily sent when available.

## API Design

In this section, we design the API for subscribing to the samples.  Configuration of sFlow/NetFlow/Ipfix is not configured over the subscription channel. The subscription channel is just for streaming.
Further, only either UDP or RPC streaming would be supported at any point in time.   It is probably easier to have the collection service support make-before-break than to have hardware support for simultaneous streaming.

### Provide Sample over gRPC

Have single gNSI RPC's in the service and include typing in response

* Pro's 
  * Single RPC entry point
  * Samples are easily serialized for consumption later
* Con's
  * Requires the most business logic on the collector and agent to handle how errors handled for requests for a protocol that isn't configured or supported is handled.


```protobuf
service gNPSI {
  // gNPSI subscription allows client to subscribe to SFlow/NetFlow/IPFIX
  // updates from the device.  Past updates, i.e., updates before the
  // subscription is received, will not be presented to the subscribing client.
  rpc Subscribe(Request) returns (stream Sample);
}

message SFlowMetadata {
  enum Version {
    UNSPECIFIED = 0;
    V2 = 1;
    V5 = 2;
  }
  Version version = 1;
}

message NetFlowMetadata {
  enum Version {
    UNSPECIFIED = 0;
    V1 = 1;
    V5 = 2;
    V7 = 3;
    V9 = 4;
  }
  Version version = 1;
}

message IPFIXMetadata {
  enum Version {
    UNSPECIFIED = 0;
    V10 = 1;
  }
  Version version = 1;
}

message Request {}

message Sample {
  // Payload of the sample.
  bytes packet = 1;

  // Last timestamp of sample payload (ns since epoch)
  int64 timestamp = 2;

  // Only one of these metadata will be populated to correspond to the sample
  // returned.
  //
  // The metadata fields applies to all messages on this stream, and would only
  // be present in the first message on the stream.
  SFlowMetadata sflow_metadata = 101;
  NetFlowMetadata netflow_metadata = 102;
  IPFIXMetadata ipfix_metadata = 103;
}

```

## Service Discovery

### Collection service discovers switches

The device provides the gRPC server, like gNMI and gNOI.  The collection system would then need a list of devices to connect to.

* Pros:
  * Congruity with gNMI and gNOI.
  * Failure to connect is available in a single location, easing debugging and triage.
* Cons:
  * Discovery is pushed onto the collection system which can leave a device disconnected.
