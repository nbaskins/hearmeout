localparam ADDRESS = 69;

module i2c_led(
    input btn_a,
    input sda_i,
    input scl_i,

    output logic sda_o,
    output logic scl_o,

    output logic led_r,
    output logic led_g,
    output logic led_b,

    // DEBUG //
    output logic state0,
    output logic state1,

    output logic dbg0,
    output logic dbg1,
    output logic dbg2,
    output logic dbg3
    // DEBUG //
);
    // access the internal gowin clock
    logic clk;
    Gowin_OSC gowin_osc (
        .oscout(clk),
        .oscen(1'b1)
    );

    // state machine
    logic [1:0] state, next_state;

    // detect the positive and negative edge of the sda and scl
    logic prev_sda, sda;
    logic prev_scl, scl;
    always_ff @(posedge clk) begin
        if(~btn_a) begin
            sda <= '0;
            scl <= '0;
            prev_sda <= '0;
            prev_scl <= '0;
        end else begin
            prev_sda <= sda;
            prev_scl <= scl;
            sda <= sda_i;
            scl <= scl_i;
        end
    end

    // edge signals
    logic rising_sda, falling_sda;
    logic rising_scl, falling_scl;

    assign rising_sda  = ~prev_sda && sda;
    assign falling_sda = prev_sda && ~sda;
    assign rising_scl  = ~prev_scl && scl;
    assign falling_scl = prev_scl && ~scl;

    // read the address
    logic [6:0] address, next_address;
    logic [3:0] address_bits_read, next_address_bits_read;
    logic address_eq;
    assign address_eq = address == ADDRESS;
    assign next_address = {address[5:0], sda};
    assign next_address_bits_read = address_bits_read + 'd1;

    always_ff @(posedge clk) begin
        if(~btn_a) begin
            address <= '0;
            address_bits_read <= '0;
        end else begin
            if(state == 2'b01 && rising_scl) begin
                // clock only the first seven bits into the address register
                if(next_address_bits_read <= 7) begin
                    address <= next_address;
                end else begin
                    address <= address;
                end

                address_bits_read <= next_address_bits_read;
            end else if(state != 2'b01) begin
                address <= '0;
                address_bits_read <= '0;
            end
        end
    end

    // read the data
    logic [7:0] data, next_data;
    logic [3:0] data_bits_read, next_data_bits_read;
    assign next_data = {data[6:0], sda};
    assign next_data_bits_read = data_bits_read + 'd1;

    always_ff @(posedge clk) begin
        if(~btn_a) begin
            data <= '0;
            data_bits_read <= '0;
        end else begin
            if(state == 2'b10 && rising_scl) begin
                // clock only the first seven bits into the address register
                if(next_data_bits_read <= 8) begin
                    data <= next_data;
                end else begin
                    data <= data;
                end

                data_bits_read <= next_data_bits_read;
            end else if(state != 2'b10) begin
                data <= '0;
                data_bits_read <= '0;
            end
        end
    end

    always_ff @(posedge clk) begin
        // ack
        if(btn_a && ((address_bits_read == 'd8 && state == 2'b01) || (data_bits_read == 'd8 && state == 2'b10)) && falling_scl) begin
            sda_o <= 1'b1;
        end else if (~btn_a || ((address_bits_read < 'd8 && state == 2'b01) || (data_bits_read < 'd8 && state == 2'b10) || state == 2'b00)) begin
            sda_o <= '0;
        end
    end

    always_comb begin
        case (state) 
            2'b00: begin
                next_state = (falling_sda && scl) ? 2'b01 : state;
            end
            2'b01: begin // (address_eq ? 2'b10 : 2'b00)
                next_state = (falling_scl && address_bits_read == 'd9) ? (address_eq ? 2'b10 : 2'b00) : state;
            end
            2'b10: begin
                next_state = (falling_scl && data_bits_read == 'd9) ? 2'b00 : state;
            end
            default: next_state = '0;
        endcase
    end

    always_ff @(posedge clk) begin
        if(~btn_a) begin
            state <= 2'b00;
        end else begin
            state <= next_state;
        end
    end

    // assign the LEDs
    always_ff @(posedge clk) begin
        if(~btn_a) begin
            led_r <= '0;
            led_g <= '0;
            led_b <= '0;
        end else begin
            if(falling_scl && data_bits_read == 'd9 && state == 2'b10) begin
                case (data) 
                    'h10: begin
                        led_r <= 0;
                    end
                    'h11: begin
                        led_r <= 1;
                    end
                    'h20: begin
                        led_g <= 0;
                    end
                    'h21: begin
                        led_g <= 1;
                    end
                    'h30: begin
                        led_b <= 0;
                    end
                    'h31: begin
                        led_b <= 1;
                    end
                endcase
            end
        end
    end

    assign state0 = state[0];
    assign state1 = state[1];

    assign dbg0 = address[0];
    assign dbg1 = address[1];
    assign dbg2 = address[2];
    assign dbg3 = address[3];

    assign scl_o = 'd0;

endmodule